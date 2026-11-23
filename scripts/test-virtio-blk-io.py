#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Check SeaBIOS virtio-blk completion I/O with the real wait function."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "src" / "hw" / "virtio-blk.c"

STUB = r"""
#include <assert.h>
#include <stddef.h>
#include <stdbool.h>

struct vring_virtqueue { bool done; };
struct vp_device { int placeholder; };
struct vring_list { int placeholder; };
struct virtiodrive_s {
    struct vring_virtqueue *vq;
    struct vp_device vp;
};

static int yield_count, kick_count, ring_count, consume_count, isr_count;
static bool immediate_complete;
static struct vring_virtqueue queue;

static void vring_add_buf(struct vring_virtqueue *q, struct vring_list *sg,
                          int out, int in, int index, int extra)
{
    assert(q == &queue && sg && out + in == 3 && index == 0 && extra == 0);
    assert(out == 2 || out == 1);
    assert(in == 3 - out);
    ring_count++;
}

static void vring_kick(struct vp_device *vp, struct vring_virtqueue *q, int n)
{
    assert(vp && q == &queue && n == 1 && ring_count == 1);
    kick_count++;
    if (immediate_complete) {
        q->done = true;
    }
}

static int vring_more_used(struct vring_virtqueue *q)
{
    assert(q == &queue && kick_count == 1);
    return q->done;
}

static void yield(void)
{
    yield_count++;
    if (yield_count == 2) {
        queue.done = true;
    }
}

static void vring_get_buf(struct vring_virtqueue *q, void *result)
{
    assert(q == &queue && result == NULL && q->done && kick_count == 1);
    consume_count++;
}

static void vp_get_isr(struct vp_device *vp)
{
    assert(vp && consume_count == 1);
    isr_count++;
}
"""

CHECKS = r"""
static void run_case(int iswrite, bool immediate)
{
    struct virtiodrive_s drive = { .vq = &queue };
    struct vring_list sg[3] = { 0 };

    queue.done = false;
    yield_count = kick_count = ring_count = consume_count = isr_count = 0;
    immediate_complete = immediate;

    virtio_blk_op_one_segment(&drive, iswrite, sg);

    assert(kick_count == 1 && ring_count == 1);
    assert(consume_count == 1 && isr_count == 1);
    assert(yield_count == (immediate ? 0 : 2));
}

int main(void)
{
    run_case(0, true);
    run_case(1, true);
    run_case(0, false);
    run_case(1, false);
    return 0;
}
"""

source = SOURCE.read_text(encoding="utf-8")
start = source.index("void\nvirtio_blk_op_one_segment(")
end = source.index("\nstatic int\nvirtio_blk_op(", start)
function = source[start:end]

# The completed descriptor must be observed before reclamation and ISR ack.
assert "while (!vring_more_used(vq))" in function
assert "usleep(" not in function
assert function.index("vring_kick(") < function.index("vring_more_used(")
assert function.index("vring_more_used(") < function.index("vring_get_buf(")
assert function.index("vring_get_buf(") < function.index("vp_get_isr(")

cc = shutil.which("cc") or shutil.which("clang") or shutil.which("gcc")
assert cc, "native C compiler required"

with tempfile.TemporaryDirectory(prefix="seabios-virtio-io-") as temp:
    program = Path(temp) / "virtio-io-test.c"
    binary = Path(temp) / "virtio-io-test"
    program.write_text(STUB + "\n" + function + "\n" + CHECKS,
                       encoding="utf-8")
    subprocess.run([cc, "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                    str(program), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

print("SeaBIOS virtio-blk polling and completion order: verified")
