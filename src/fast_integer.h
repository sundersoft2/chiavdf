#define TEST_FAST_INTEGER 0

template<int d_expected_size, int d_padded_size> struct alignas(64) mpz_fast {
    static const int expected_size=d_expected_size;
    static const int padded_size=d_padded_size;

    static_assert(expected_size<=padded_size, "");

    uint64 data[padded_size+1];

    #if TEST_FAST_INTEGER
        typedef mpz<expected_size, padded_size+1> mpz_slow;
        mpz_slow c_slow;

        void validate_slow() const {
            mpz_struct s_slow=get_mpz_struct();
            assert(c_slow.equal(&s_slow));
        }
    #endif

    mpz_fast() {
        for (int i=0;i<=padded_size;++i) {
            data[i]=0;
        }

        #if TEST_FAST_INTEGER
            c_slow=uint64(0);
        #endif
    }

    //~0: negative; 0: positive
    uint64& get_sign_limb() {
        return data[padded_size];
    }

    const uint64& get_sign_limb() const {
        return data[padded_size];
    }

    int get_num_limbs(bool should_validate_slow=true) const {
        int res=asm_code::asm_avx2_func_fast_integer_size_limbs<expected_size, padded_size>(data);
        //todo int res=0;

        #if TEST_FAST_INTEGER
            int num_limbs=0;

            for (int i=expected_size-1;i>=0;--i) {
                if (data[i]!=0) {
                    num_limbs=i+1;
                    break;
                }
            }

            //todo res=num_limbs;

            assert(res==num_limbs);

            if (should_validate_slow) {
                assert(res==c_slow.num_limbs());
            }
        #endif

        return res;
    }

    mpz_struct get_mpz_struct() const {
        int abs_size=get_num_limbs(false);
        int sign_size=(int(get_sign_limb())<<1) + 1; //-1 if negative, 1 if positive

        mpz_struct c_mpz;
        c_mpz._mp_alloc=expected_size;
        c_mpz._mp_size=abs_size * sign_size;
        c_mpz._mp_d=(uint64*)data;

        return c_mpz;
    }

    //call this after GMP writes to the integer
    void finish_mpz_struct(const mpz_struct& s) {
        int signed_size=s._mp_size;

        assert(s._mp_d==(uint64*)data); //reallocation not allowed

        get_sign_limb()=uint64(int64(signed_size)>>63); //-1 if negative, 0 if positive

        int abs_size=signed_size;
        if (abs_size<0) {
            abs_size=-abs_size;
        }

        assert(abs_size<=expected_size);

        for (int i=abs_size;i<expected_size;++i) {
            data[i]=0;
        }
    }

    template<int t_expected_size, int t_padded_size> void assign_fast_only(const mpz_fast<t_expected_size, t_padded_size>& t) {
        static_assert(ceil_div(expected_size, 4)<padded_size, ""); //can load all of the data with AVX2 ops without loading the sign limb

        int i=0;
        for (;i<t_expected_size && i<expected_size;i+=4) {
            __m256i d=_mm256_loadu_si256((__m256i*)(t.data + i));
            _mm256_storeu_si256((__m256i*)(data + i), d);
        }

        uint64 zero[4]={0, 0, 0, 0};
        __m256i d_zero=_mm256_loadu_si256((__m256i*)zero);

        for (;i<expected_size;i+=4) {
            _mm256_storeu_si256((__m256i*)(data + i), d_zero);
        }

        get_sign_limb()=t.get_sign_limb();
    }

    template<int t_expected_size, int t_padded_size> mpz_fast& operator=(const mpz_fast<t_expected_size, t_padded_size>& t) {
        #if TEST_FAST_INTEGER
            t.validate_slow();
            assert(((void*)&t)!=((void*)this));
        #endif

        assign_fast_only(t);

        #if TEST_FAST_INTEGER
            c_slow=t.c_slow;
            validate_slow();
        #endif

        return *this;
    }

    mpz_fast& operator=(uint64 t) {
        data[0]=t;

        for (int i=1;i<expected_size;++i) {
            data[i]=0;
        }

        get_sign_limb()=0;

        #if TEST_FAST_INTEGER
            c_slow=t;
            validate_slow();
        #endif

        return *this;
    }

    mpz_fast& operator=(const mpz_struct* t) {
        mpz_struct s=get_mpz_struct();
        mpz_set(&s, t);
        finish_mpz_struct(s);

        #if TEST_FAST_INTEGER
            c_slow=t;
            validate_slow();
        #endif

        return *this;
    }

    void set(mpz_struct* t) const {
        #if TEST_FAST_INTEGER
            validate_slow();
        #endif

        mpz_struct s=get_mpz_struct();
        mpz_set(t, &s);
    }

    bool is_same(const mpz_fast& t) const {
        bool is_zero=true;

        for (int i=0;i<=padded_size;++i) {
            if (i<padded_size && data[i]!=0ull) {
                is_zero=false;
            }

            if ((i<padded_size || !is_zero) && data[i]!=t.data[i]) {
                return false;
            }

            if (i>=expected_size && i<padded_size) {
                if (data[i]!=0) {
                    return false;
                }
            }

            if (i==padded_size) {
                if (data[i]!=0 && data[i]!=~(0ull)) {
                    return false;
                }
            }
        }

        return true;
    }

    void assert_same(const mpz_fast& t) const {
        assert(is_same(t));
    }

    template<int t_expected_size_a, int t_padded_size_a, int t_expected_size_b, int t_padded_size_b>
    void set_add(const mpz_fast<t_expected_size_a, t_padded_size_a>& a, const mpz_fast<t_expected_size_b, t_padded_size_b>& b) {
        #if TEST_FAST_INTEGER
            a.validate_slow();
            b.validate_slow();

            mpz_fast copy=*this;
            mpz_struct s=copy.get_mpz_struct();
            mpz_struct as=a.get_mpz_struct();
            mpz_struct bs=b.get_mpz_struct();
            mpz_add(&s, &as, &bs);
            copy.finish_mpz_struct(s);
        #endif

        asm_code::asm_avx2_func_fast_integer_add
            <expected_size, padded_size, t_expected_size_a, t_padded_size_a, t_expected_size_b, t_padded_size_b>
        (data, a.data, b.data, 0ull);

        #if TEST_FAST_INTEGER
            //todo assign_fast_only(copy);

            assert_same(copy);
            c_slow.set_add(a.c_slow, b.c_slow);
            validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a, int t_expected_size_b, int t_padded_size_b>
    void set_sub(const mpz_fast<t_expected_size_a, t_padded_size_a>& a, const mpz_fast<t_expected_size_b, t_padded_size_b>& b) {
        #if TEST_FAST_INTEGER
            a.validate_slow();
            b.validate_slow();

            mpz_fast<t_expected_size_a, t_padded_size_a> a_copy=a;
            mpz_fast<t_expected_size_a, t_padded_size_a> b_copy=b;

            mpz_fast copy=*this;
            mpz_struct s=copy.get_mpz_struct();
            mpz_struct as=a.get_mpz_struct();
            mpz_struct bs=b.get_mpz_struct();
            mpz_sub(&s, &as, &bs);
            copy.finish_mpz_struct(s);
        #endif

        asm_code::asm_avx2_func_fast_integer_add
            <expected_size, padded_size, t_expected_size_a, t_padded_size_a, t_expected_size_b, t_padded_size_b>
        (data, a.data, b.data, ~0ull);

        #if TEST_FAST_INTEGER
            //todo assign_fast_only(copy);

            assert_same(copy);
            c_slow.set_sub(a.c_slow, b.c_slow);
            validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a, int t_expected_size_b, int t_padded_size_b>
    void set_mul(const mpz_fast<t_expected_size_a, t_padded_size_a>& a, const mpz_fast<t_expected_size_b, t_padded_size_b>& b) {
        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();
            b.validate_slow();

            assert(((void*)&a)!=((void*)this));
            assert(((void*)&b)!=((void*)this));

            mpz_fast copy=*this;
            mpz_struct s=copy.get_mpz_struct();
            mpz_struct as=a.get_mpz_struct();
            mpz_struct bs=b.get_mpz_struct();
            mpz_mul(&s, &as, &bs);
            copy.finish_mpz_struct(s);
        #endif

        asm_code::asm_avx2_func_fast_integer_mul
            <expected_size, padded_size, t_expected_size_a, t_padded_size_a, t_expected_size_b, t_padded_size_b>
        (data, a.data, b.data);

        #if TEST_FAST_INTEGER
            //todo assign_fast_only(copy);

            if (!is_same(copy)) {
                asm_code::asm_avx2_func_fast_integer_mul
                    <expected_size, padded_size, t_expected_size_a, t_padded_size_a, t_expected_size_b, t_padded_size_b>
                (data, a.data, b.data);

                assert(false);
            }

            assert_same(copy);
            c_slow.set_mul(a.c_slow, b.c_slow);
            validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a>
    void set_mul(const mpz_fast<t_expected_size_a, t_padded_size_a>& a, uint64 b) {
        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();
            assert(((void*)&a)!=((void*)this));

            mpz_fast copy=*this;
            mpz_struct s=copy.get_mpz_struct();
            mpz_struct as=a.get_mpz_struct();
            mpz_mul_ui(&s, &as, b);
            copy.finish_mpz_struct(s);
        #endif

        asm_code::asm_avx2_func_fast_integer_mul_add
            <expected_size, padded_size, t_expected_size_a, t_padded_size_a>
        (data, a.data, b);

        get_sign_limb()=a.get_sign_limb();

        #if TEST_FAST_INTEGER
            //todo assign_fast_only(copy);

            assert_same(copy);
            c_slow.set_mul(a.c_slow, b);
            validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a>
    void set_sub_mul(const mpz_fast<t_expected_size_a, t_padded_size_a>& a, uint64 b, mpz_fast& buffer) {
        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();
            buffer.validate_slow();

            assert(((void*)&a)!=((void*)this));
            assert(((void*)&buffer)!=((void*)this));

            mpz_slow c_slow_2=c_slow;
            c_slow_2.set_sub_mul(a.c_slow, b, nullptr);
        #endif

        buffer.set_mul(a, b);
        set_sub(*this, buffer);

        #if TEST_FAST_INTEGER
            assert(c_slow.equal(c_slow_2));
        #endif
    }

    void negate() {
        #if TEST_FAST_INTEGER
            validate_slow();
        #endif

        get_sign_limb()^=~(0ull);

        #if TEST_FAST_INTEGER
            c_slow.negate();
            validate_slow();
        #endif
    }

    void abs() {
        #if TEST_FAST_INTEGER
            validate_slow();
        #endif

        get_sign_limb()=0ull;

        #if TEST_FAST_INTEGER
            c_slow.abs();
            validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a, int t_expected_size_b, int t_padded_size_b, int t_expected_size_c, int t_padded_size_c>
    void set_divide_floor(
        const mpz_fast<t_expected_size_a, t_padded_size_a>& a,
        const mpz_fast<t_expected_size_b, t_padded_size_b>& b,
        mpz_fast<t_expected_size_c, t_padded_size_c>& remainder
    ) {
        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();
            b.validate_slow();
            remainder.validate_slow();

            assert(((void*)&a)!=((void*)this));
            assert(((void*)&b)!=((void*)this));
            assert(((void*)&a)!=((void*)&remainder));
            assert(((void*)&b)!=((void*)&remainder));
            assert(((void*)&remainder)!=((void*)this));
        #endif

        mpz_struct s=get_mpz_struct();
        mpz_struct as=a.get_mpz_struct();
        mpz_struct bs=b.get_mpz_struct();
        mpz_struct remainder_s=remainder.get_mpz_struct();
        mpz_fdiv_qr(&s, &remainder_s, &as, &bs);
        finish_mpz_struct(s);
        remainder.finish_mpz_struct(remainder_s);

        #if TEST_FAST_INTEGER
            c_slow.set_divide_floor(a.c_slow, b.c_slow, remainder.c_slow);
            validate_slow();
            remainder.validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a, int t_expected_size_b, int t_padded_size_b>
    void set_divide_exact(const mpz_fast<t_expected_size_a, t_padded_size_a>& a, const mpz_fast<t_expected_size_b, t_padded_size_b>& b) {
        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();
            b.validate_slow();

            assert(((void*)&a)!=((void*)this));
            assert(((void*)&b)!=((void*)this));
        #endif

        mpz_struct s=get_mpz_struct();
        mpz_struct as=a.get_mpz_struct();
        mpz_struct bs=b.get_mpz_struct();
        mpz_divexact(&s, &as, &bs);
        finish_mpz_struct(s);

        #if TEST_FAST_INTEGER
            c_slow.set_divide_exact(a.c_slow, b.c_slow);
            validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a, int t_expected_size_b, int t_padded_size_b>
    void set_mod(const mpz_fast<t_expected_size_a, t_padded_size_a>& a, const mpz_fast<t_expected_size_b, t_padded_size_b>& b) {
        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();
            b.validate_slow();

            assert(((void*)&a)!=((void*)this));
            assert(((void*)&b)!=((void*)this));
        #endif

        mpz_struct s=get_mpz_struct();
        mpz_struct as=a.get_mpz_struct();
        mpz_struct bs=b.get_mpz_struct();
        mpz_mod(&s, &as, &bs);
        finish_mpz_struct(s);

        #if TEST_FAST_INTEGER
            c_slow.set_mod(a.c_slow, b.c_slow);
            validate_slow();
        #endif
    }

    template<int t_expected_size_a, int t_padded_size_a>
    mpz_fast& operator%=(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) {
        set_mod(*this, a);
        return *this;
    }

    template<int t_expected_size_a, int t_padded_size_a>
    int compare_abs(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const {
        int res=asm_code::asm_avx2_func_fast_integer_cmp_abs_1<expected_size, padded_size, t_expected_size_a, t_padded_size_a>(data, a.data);
        //int res=0;

        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();

            mpz_struct s=get_mpz_struct();
            mpz_struct as=a.get_mpz_struct();

            //todo res=mpz_cmpabs(&s, &as);

            if (res!=mpz_cmpabs(&s, &as)) {
                asm_code::asm_avx2_func_fast_integer_cmp_abs_1<expected_size, padded_size, t_expected_size_a, t_padded_size_a>(data, a.data);
            }

            assert(res==mpz_cmpabs(&s, &as));
            assert(res==c_slow.compare_abs(a.c_slow));
        #endif

        return res;
    }

    template<int t_expected_size_a, int t_padded_size_a>
    int compare(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const {
        int res=asm_code::asm_avx2_func_fast_integer_cmp<expected_size, padded_size, t_expected_size_a, t_padded_size_a>(data, a.data);
        //int res=0;

        #if TEST_FAST_INTEGER
            validate_slow();
            a.validate_slow();

            mpz_struct s=get_mpz_struct();
            mpz_struct as=a.get_mpz_struct();

            //todo res=mpz_cmp(&s, &as);

            assert(res==mpz_cmp(&s, &as));
            assert(res==c_slow.compare(a.c_slow));
        #endif

        return res;
    }

    template<int t_expected_size_a, int t_padded_size_a>
    bool operator<(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const { return compare(a)<0; }

    template<int t_expected_size_a, int t_padded_size_a>
    bool operator<=(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const { return compare(a)<=0; }

    template<int t_expected_size_a, int t_padded_size_a>
    bool equal(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const { return compare(a)==0; }

    template<int t_expected_size_a, int t_padded_size_a>
    bool operator>=(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const { return compare(a)>=0; }

    template<int t_expected_size_a, int t_padded_size_a>
    bool operator>(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const { return compare(a)>0; }

    template<int t_expected_size_a, int t_padded_size_a>
    bool not_equal(const mpz_fast<t_expected_size_a, t_padded_size_a>& a) const { return compare(a)!=0; }

    bool is_negative_nonzero() const {
        bool res=(get_sign_limb()!=0);
        
        #if TEST_FAST_INTEGER
            assert(res==c_slow.is_negative_nonzero());
        #endif

        return res;
    }

    bool is_nonzero() const {
        bool res=(get_num_limbs()!=0);

        #if TEST_FAST_INTEGER
            assert(res==c_slow.is_nonzero());
        #endif

        return res;
    }

    int num_bits() const {
        mpz_struct s=get_mpz_struct();
        int res=mpz_sizeinbase(&s, 2);

        #if TEST_FAST_INTEGER
            assert(res==c_slow.num_bits());
        #endif

        return res;
    }

    //0 if this is 0
    int num_limbs() const {
        return get_num_limbs();
    }

    const uint64* read_limbs() const {
        return data;
    }

    //limbs are uninitialized. call finish
    template<int num> uint64* write_limbs() {
        return data;
    }

    //limbs are zero padded to the specified size. call finish
    template<int num> uint64* modify_limbs() {
        return data;
    }

    //num is whatever was passed to write_limbs or modify_limbs
    //it can be less than that as long as it is at least the number of nonzero limbs
    //it can be 0 if the result is 0
    template<int num, bool negative> void finish() {
        for (int x=num;x<expected_size;++x) {
            data[x]=0;
        }

        get_sign_limb()=(negative)? (~0ull) : 0ull;

        #if TEST_FAST_INTEGER
            mpz_struct s=get_mpz_struct();
            c_slow=&s;
        #endif
    }

    template<int size> array<uint64, size> to_array() const {
        array<uint64, size> res;

        int x=0;
        for (;x<size && x<expected_size;++x) {
            res[x]=data[x];
        }

        for (;x<size;++x) {
            res[x]=0;
        }

        return res;
    }
};