/* GPL-3.0-or-later. Behavioral checks against the actual candidate helper.
 * The embedded implementation retains Klipper's original copyright notice. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static long allocations;
static void *counted_malloc(size_t size) {
    void *p = malloc(size);
    assert(p);
    allocations++;
    return p;
}
static void counted_free(void *p) {
    if (p) allocations--;
    free(p);
}
#define malloc counted_malloc
#define free counted_free
#include "kin_extruder.c"
#undef malloc
#undef free

/* Exercise the full triangular smoothing window across a PA-change boundary.
 * Integrating the PA step analytically gives .02, .0275, .05, .0725, .08.
 * Repeat at a large absolute print clock, retaining local move-time arithmetic. */
static void check_smoothing(double clock) {
    struct stepper_kinematics *sk = extruder_stepper_alloc();
    extruder_set_pressure_advance(sk, clock, .02, .04);
    extruder_set_pressure_advance(sk, clock + 2., .08, .04);
    struct list_head moves;
    list_init(&moves);
    struct move m[3] = {{0}};
    for (int i = 0; i < 3; i++) {
        m[i].print_time = clock + 1. + i;
        m[i].move_t = 1.; m[i].start_v = 1.; m[i].axes_r.y = 1.;
        m[i].start_pos.x = 1. + i;
        list_add_tail(&m[i].node, &moves);
    }
    double positions[] = {1.5, 1.99, 2., 2.01, 2.5};
    double advances[] = {.02, .0275, .05, .0725, .08};
    for (int i = 0; i < 5; i++) {
        int index = positions[i] < 2. ? 0 : 1;
        double actual = extruder_calc_position(sk, &m[index], positions[i] - 1. - index);
        assert(fabs(actual - positions[i] - advances[i]) < 1e-8);
    }
    /* A flushed smoothing-window disable is nominal motion, then can re-enable.
     * The caller must flush before changing the window, as QIDI toolhead does. */
    sk->last_flush_time = clock + 3.;
    extruder_set_pressure_advance(sk, clock + 3., 0., 0.);
    assert(fabs(extruder_calc_position(sk, &m[1], .5) - 2.5) < 1e-10);
    extruder_set_pressure_advance(sk, clock + 3., .08, .04);
    assert(fabs(extruder_calc_position(sk, &m[2], .5) - 3.58) < 1e-8);
    extruder_stepper_free(sk);
    assert(allocations == 0);
}

static void check_queued_history(void) {
    struct stepper_kinematics *sk = extruder_stepper_alloc();
    for (int i = 1; i <= 1000; i++)
        extruder_set_pressure_advance(sk, i, i % 2 ? .02 : .08, .04);
    /* Future history must be retained until generated motion no longer needs it.
     * The leak fix is not an arbitrary global cap on pending PA commands. */
    assert(allocations == 1002);
    long pending = allocations;
    extruder_set_pressure_advance(sk, 1001., .08, .04);
    assert(allocations == pending); /* identical PA is deduplicated */
    sk->last_flush_time = 1002.;
    extruder_set_pressure_advance(sk, 1003., .02, .04);
    assert(allocations <= 3);
    extruder_stepper_free(sk);
    assert(allocations == 0);
}

int main(void) {
    check_smoothing(0.);
    check_smoothing(1000000.);
    check_queued_history();
    struct stepper_kinematics *sk = extruder_stepper_alloc();
    struct extruder_stepper *es = container_of(sk, struct extruder_stepper, sk);
    extruder_set_pressure_advance(sk, 0., .02, .04);
    extruder_set_pressure_advance(sk, 2., .08, .04);
    struct move before = {0}, after = {0};
    before.print_time = 1.; before.move_t = 1.;
    before.start_v = 1.; before.axes_r.y = 1.;
    after = before; after.print_time = 2.;
    /* A queued future update must not change the earlier move's PA. */
    /* Integral t*(t+PA) from 0 to 1 is 1/3 + PA/2. */
    assert(fabs(pa_move_integrate(&before, &es->pa_list, 0., 0., 1., 0.) - (1./3. + .01)) < 1e-10);
    assert(fabs(pa_move_integrate(&after, &es->pa_list, 0., 0., 1., 0.) - (1./3. + .04)) < 1e-10);
    /* PA also changes the velocity term under acceleration, not just position. */
    before.half_accel = .5;
    assert(fabs(pa_move_integrate(&before, &es->pa_list, 0., 0., 1., 0.) - (1./8. + 1./3. + .02*5./6.)) < 1e-10);
    /* Retraction / non-extruding XY moves must not receive PA. */
    before.axes_r.y = 0.;
    assert(fabs(pa_move_integrate(&before, &es->pa_list, 0., 0., 1., 0.) - (1./8. + 1./3.)) < 1e-10);
    for (int i = 3; i < 10003; i++) {
        sk->last_flush_time = i;
        extruder_set_pressure_advance(sk, i + .1, i % 2 ? .02 : .08, .04);
        assert(allocations <= 4);
    }
    extruder_stepper_free(sk);
    assert(allocations == 0);
    puts("PASS: timed PA, boundary smoothing, large clock, non-PA moves, queued history, expired cleanup and full deallocation");
    return 0;
}
