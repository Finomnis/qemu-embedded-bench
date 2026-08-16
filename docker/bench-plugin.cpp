extern "C"
{
#include <qemu-plugin.h>
}

#include <cinttypes>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

struct State
{
    uint64_t start_addr = 0;
    uint64_t end_addr = 0;

    bool active = false;
    uint64_t regions_started = 0;
    uint64_t regions_completed = 0;

    uint64_t instructions = 0;
    uint64_t reads = 0;
    uint64_t writes = 0;

    bool full_trace = false;

    std::vector<uint64_t> trace = {};
};

static State *state = nullptr;

const uint64_t TRACE_ENTRY_MEM_WRITE = (uint64_t)-2;
const uint64_t TRACE_ENTRY_MEM_READ = (uint64_t)-3;

/*
 * Called immediately before an instrumented guest instruction executes.
 *
 * userdata contains that instruction's guest PC.
 */
static void
on_instruction(unsigned int vcpu_index, void *userdata)
{
    (void)vcpu_index;

    uint64_t pc = (uint64_t)(uintptr_t)userdata;

    /*
     * The marker instructions themselves are deliberately not counted.
     */
    if (pc == state->start_addr)
    {
        state->active = true;
        state->regions_started++;
    }

    if (pc == state->end_addr)
    {
        state->active = false;
        state->regions_completed++;
    }

    if (state->active)
    {
        state->instructions++;

        if (state->full_trace)
        {
            state->trace.push_back(pc);
        }
    }
}

/*
 * Called for each memory access performed by an instrumented instruction.
 *
 * A single instruction can invoke this more than once, e.g. an instruction
 * that accesses multiple memory locations.
 */
static void on_memory(unsigned int vcpu_index,
                      qemu_plugin_meminfo_t info,
                      uint64_t vaddr,
                      void *userdata)
{
    (void)vcpu_index;
    (void)vaddr;
    (void)userdata;

    if (!state->active)
    {
        return;
    }

    if (qemu_plugin_mem_is_store(info))
    {
        state->writes++;

        if (state->full_trace)
        {
            state->trace.push_back(TRACE_ENTRY_MEM_WRITE);
        }
    }
    else
    {
        state->reads++;

        if (state->full_trace)
        {
            state->trace.push_back(TRACE_ENTRY_MEM_READ);
        }
    }
}

/*
 * Called whenever QEMU translates a Translation Block.
 *
 * Important: we aren't counting translations here. We use this opportunity
 * only to attach runtime callbacks to each guest instruction.
 */
static void on_tb_translate(qemu_plugin_id_t id,
                            struct qemu_plugin_tb *tb)
{
    (void)id;

    size_t count = qemu_plugin_tb_n_insns(tb);

    for (size_t i = 0; i < count; i++)
    {
        struct qemu_plugin_insn *insn =
            qemu_plugin_tb_get_insn(tb, i);

        uint64_t pc = qemu_plugin_insn_vaddr(insn);

        qemu_plugin_register_vcpu_insn_exec_cb(
            insn,
            on_instruction,
            QEMU_PLUGIN_CB_NO_REGS,
            (void *)(uintptr_t)pc);

        qemu_plugin_register_vcpu_mem_cb(
            insn,
            on_memory,
            QEMU_PLUGIN_CB_NO_REGS,
            QEMU_PLUGIN_MEM_RW,
            NULL);
    }
}

static void on_exit(qemu_plugin_id_t id, void *userdata)
{
    (void)id;
    (void)userdata;

    char output[512];

    if (state->full_trace)
    {
        for (const auto pc : state->trace)
        {
            if (pc == TRACE_ENTRY_MEM_READ)
            {
                qemu_plugin_outs("r\n");
            }
            else if (pc == TRACE_ENTRY_MEM_WRITE)
            {
                qemu_plugin_outs("w\n");
            }
            else
            {
                snprintf(output, sizeof(output), "0x%" PRIx64 "\n", pc);
                qemu_plugin_outs(output);
            }
        }
    }
    else
    {
        snprintf(
            output,
            sizeof(output),
            "{\n"
            "  \"regions_started\": %" PRIu64 ",\n"
            "  \"regions_completed\": %" PRIu64 ",\n"
            "  \"instructions\": %" PRIu64 ",\n"
            "  \"reads\": %" PRIu64 ",\n"
            "  \"writes\": %" PRIu64 "\n"
            "}\n",
            state->regions_started,
            state->regions_completed,
            state->instructions,
            state->reads,
            state->writes);

        qemu_plugin_outs(output);
    }

    delete state;
    state = nullptr;
}

static bool parse_address(const char *arg,
                          const char *name,
                          uint64_t *result)
{
    size_t name_len = strlen(name);

    if (strncmp(arg, name, name_len) != 0 ||
        arg[name_len] != '=')
    {
        return false;
    }

    const char *value = arg + name_len + 1;

    char *end;
    unsigned long long parsed = strtoull(value, &end, 0);

    if (*value == '\0' || *end != '\0')
    {
        fprintf(stderr, "invalid address: %s\n", arg);
        return false;
    }

    *result = (uint64_t)parsed;
    return true;
}

static bool parse_option(const char *arg,
                         const char *name)
{
    size_t name_len = strlen(name);
    return strncmp(arg, name, name_len) == 0;
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id,
                        const qemu_info_t *info,
                        int argc,
                        char **argv)
{
    state = new State;

    bool have_start = false;
    bool have_end = false;

    for (int i = 0; i < argc; i++)
    {
        if (parse_address(argv[i], "start", &(state->start_addr)))
        {
            have_start = true;
        }
        else if (parse_address(argv[i], "end", &(state->end_addr)))
        {
            have_end = true;
        }
        else if (parse_option(argv[i], "trace"))
        {
            state->full_trace = true;
        }
        else
        {
            fprintf(stderr,
                    "bench-plugin: unknown argument: %s\n",
                    argv[i]);
            return -1;
        }
    }

    if (!have_start || !have_end)
    {
        fprintf(stderr,
                "bench-plugin: expected start=<addr>,end=<addr>\n");
        return -1;
    }

    /*
     * We're using this for Cortex-M/Thumb.
     *
     * Thumb function-symbol addresses can carry bit 0 as the Thumb-state
     * indicator, whereas actual instruction addresses are aligned.
     */
    if (strcmp(info->target_name, "arm") == 0)
    {
        state->start_addr &= ~UINT64_C(1);
        state->end_addr &= ~UINT64_C(1);
    }

    qemu_plugin_register_vcpu_tb_trans_cb(
        id,
        on_tb_translate);

    qemu_plugin_register_atexit_cb(
        id,
        on_exit,
        NULL);

    return 0;
}
