#ifndef AQB48_FP_CMP_H
#define AQB48_FP_CMP_H

template<int W, int E>
ac_int<4, false> fp_arelb(const ac_std_float<W, E>& a,
                          const ac_std_float<W, E>& b) {
    ac_int<1, false> zctr(0);
    ac_int<4, false> arelb;
    ac_std_float<W, E> z0;
    ac_std_float<W, E> z1;
    fp_cmp<0>(a, b, zctr, arelb, z0, z1);
    return arelb;
}

template<int W, int E>
bool FP_LEQ(const ac_std_float<W, E>& a,
            const ac_std_float<W, E>& b) {
    ac_int<4, false> arelb = fp_arelb(a, b);
    return arelb[0] || arelb[1];
}

template<int W, int E>
bool FP_GEQ(const ac_std_float<W, E>& a,
            const ac_std_float<W, E>& b) {
    ac_int<4, false> arelb = fp_arelb(a, b);
    return arelb[0] || arelb[2];
}

#endif
