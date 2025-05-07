#ifndef AQB48_SIM_ASSERT_H
#define AQB48_SIM_ASSERT_H

#include <ac_assert.h>

#ifdef __SYNTHESIS__
#define sim_assert(expr) ((void)0)
#else
#define sim_assert(expr) assert(expr)
#endif  // __SYNTHESIS__

#endif  // AQB48_SIM_ASSERT_H
