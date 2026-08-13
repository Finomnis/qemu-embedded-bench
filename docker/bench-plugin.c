#include <qemu-plugin.h>

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static uint64_t start_addr;
static uint64_t end_addr;

static bool active;
static uint64_t regions_started;
static uint64_t regions_completed;

static uint64_t instructions;
static uint64_t reads;
static uint64_t writes;

/*
 * Called immediately before an instrumented guest instruction executes.
 *
 * userdata contains that instruction's guest PC.
 */
static void on_instruction(unsigned int vcpu_index, void *userdata)
{
    (void)vcpu_index;

    uint64_t pc = (uint64_t)(uintptr_t)userdata;

    /*
     * The marker instructions themselves are deliberately not counted.
     */
    if (pc == start_addr)
    {
        active = true;
        regions_started++;
    }

    if (pc == end_addr)
    {
        active = false;
        regions_completed++;
    }

    if (active)
    {
        instructions++;
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

    if (!active)
    {
        return;
    }

    if (qemu_plugin_mem_is_store(info))
    {
        writes++;
    }
    else
    {
        reads++;
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
        regions_started,
        regions_completed,
        instructions,
        reads,
        writes);

    qemu_plugin_outs(output);
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

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id,
                        const qemu_info_t *info,
                        int argc,
                        char **argv)
{
    bool have_start = false;
    bool have_end = false;

    for (int i = 0; i < argc; i++)
    {
        if (parse_address(argv[i], "start", &start_addr))
        {
            have_start = true;
        }
        else if (parse_address(argv[i], "end", &end_addr))
        {
            have_end = true;
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
        start_addr &= ~UINT64_C(1);
        end_addr &= ~UINT64_C(1);
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
