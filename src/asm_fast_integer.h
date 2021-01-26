namespace asm_code { namespace fast_integer {


struct fast_asm_integer {
    //the data limbs are after this address. the sign limb is after all of the data limbs (if it exists)
    //the sign limb is ~0 if negative and 0 if positive
    reg_scalar addr_base;
    int size=0; //limbs. lsb limb is first.
    int padded_size=0; //any limbs after size are 0
    bool has_sign=false;

    fast_asm_integer() {}
    fast_asm_integer(reg_scalar t_addr_base, int t_size, int t_padded_size=-1, bool t_has_sign=false) {
        addr_base=t_addr_base;
        size=t_size;
        padded_size=(t_padded_size==-1)? t_size : t_padded_size;
        has_sign=t_has_sign;

        assert(padded_size>=size);
    }

    bool is_null() {
        return size==0;
    }

    string operator[](int pos) {
        assert(pos>=0 && (pos<padded_size || pos==padded_size && has_sign));
        return str( "[#+#]", addr_base.name(), to_hex((pos)*8) );
    }

    string sign() {
        return (*this)[padded_size];
    }

    bool operator<(const fast_asm_integer& t) const {
        return
            make_tuple(addr_base.value, size, padded_size, has_sign)<
            make_tuple(t.addr_base.value, t.size, t.padded_size, t.has_sign)
        ;
    }
};

//aliasing is allowed. c can be null.
//zero does not need to be initialized. it is 0 when this returns
//out=a*b+c
void mul_add(
    reg_alloc regs, fast_asm_integer out, fast_asm_integer a, reg_scalar b, fast_asm_integer c,
    bool invert_output=false, bool carry_in_is_1=false
) {
    EXPAND_MACROS_SCOPE;

    assert(b.value==reg_rdx.value);

    //4x scalar
    reg_scalar mul_low_0=regs.bind_scalar(m, "mul_low_0");
    reg_scalar mul_low_1=regs.bind_scalar(m, "mul_low_1");
    reg_scalar mul_high_0=regs.bind_scalar(m, "mul_high_0");
    reg_scalar mul_high_1=regs.bind_scalar(m, "mul_high_1");

    //clears OF and CF
    APPEND_M(str( "XOR `mul_low_0, `mul_low_0" ));

    if (carry_in_is_1) {
        APPEND_M(str( "STC" ));
    }

    assert(a.size>=2);
    bool mul_high_0_is_zero=false;

    for (int pos=0;pos<out.size;pos+=2) {
        bool first=(pos==0);
        bool last=(pos==out.size-1 || pos==out.size-2);
        bool partial=(pos==out.size-1);

        //mul_low=mul_low+mul_high>>64
        //the size a*b is a.size+1 and any limbs after that are 0
        if (pos+1<a.size) {
            APPEND_M(str( "MULX `mul_high_0, `mul_low_0, #", a[pos] ));
            mul_high_0_is_zero=false;

            if (!first) {
                APPEND_M(str( "ADOX `mul_low_0, `mul_high_1" ));
            }

            if (!partial) {
                APPEND_M(str( "MULX `mul_high_1, `mul_low_1, #", a[pos+1] ));
                APPEND_M(str( "ADOX `mul_low_1, `mul_high_0" ));
            }
        } else
        if (pos<a.size) {
            assert(!first);
            APPEND_M(str( "MULX `mul_low_1, `mul_low_0, #", a[pos] ));
            APPEND_M(str( "ADOX `mul_low_0, `mul_high_1" ));

            if (!partial) {
                APPEND_M(str( "MOV `mul_high_1, 0" ));
                APPEND_M(str( "ADOX `mul_low_1, `mul_high_1" ));
            }
        } else {
            assert(!first);
            //OF is 0 after a.size+1 limbs
            APPEND_M(str( "MOV `mul_low_0, 0" ));
            APPEND_M(str( "ADOX `mul_low_0, `mul_high_1" ));

            if (!partial) {
                APPEND_M(str( "MOV `mul_low_1, `mul_low_0" ));

                if (!last) {
                    APPEND_M(str( "MOV `mul_high_1, `mul_low_0" ));
                }
            }
        }

        if (!c.is_null()) {
            if (pos>=c.size && !mul_high_0_is_zero) {
                //the CF flag can still be 1
                APPEND_M(str( "MOV `mul_high_0, 0" ));
                mul_high_0_is_zero=true;
            }
            APPEND_M(str( "ADCX `mul_low_0, #", (pos<c.size)? c[pos] : "`mul_high_0" ));

            if (!partial) {
                if (pos+1>=c.size && !mul_high_0_is_zero) {
                    APPEND_M(str( "MOV `mul_high_0, 0" ));
                    mul_high_0_is_zero=true;
                }
                APPEND_M(str( "ADCX `mul_low_1, #", (pos+1<c.size)? c[pos+1] : "`mul_high_0" ));
            }
        }

        if (invert_output) {
            APPEND_M(str( "NOT `mul_low_0" ));
        }
        APPEND_M(str( "MOV #, `mul_low_0", out[pos] ));

        if (pos+1<out.size) {
            assert(!partial);

            if (invert_output) {
                APPEND_M(str( "NOT `mul_low_1" ));
            }
            APPEND_M(str( "MOV #, `mul_low_1", out[pos+1] ));
        }
    }
}

void mul_add_call(
    reg_alloc regs, fast_asm_integer out, fast_asm_integer a, reg_scalar b, fast_asm_integer c,
    bool invert_output=false, bool carry_in_is_1=false
) {
    EXPAND_MACROS_SCOPE;

    static map<tuple<set<int>, fast_asm_integer, fast_asm_integer, int, fast_asm_integer, bool, bool>, string> function_map;

    string& name=function_map[make_tuple(regs.scalars, out, a, b.value, c, invert_output, carry_in_is_1)];

    if (name.empty()) {
        name=m.alloc_label();

        #ifdef CHIAOSX
            APPEND_M(str( ".text " ));
        #else
            APPEND_M(str( ".text 1" ));
        #endif

        APPEND_M(str( "#:", name ));
        mul_add(regs, out, a, b, c, invert_output, carry_in_is_1);
        APPEND_M(str( "RET" ));
        APPEND_M(str( ".text" ));
    }

    APPEND_M(str( "CALL #", name ));
}

//|a|==|b|: CF=0, ZF=1
//|a|<|b|: CF=1, ZF=0
//|a|>|b|: CF=0, ZF=0
void cmp_abs(reg_alloc regs, fast_asm_integer a, fast_asm_integer b) {
    EXPAND_MACROS_SCOPE;

    //1x scalar
    reg_scalar temp=regs.bind_scalar(m, "temp");

    string early_exit_label=m.alloc_label();

    const int unroll_factor=4;

    for (int pos=max(a.size, b.size)-1;pos>=0;--pos) {
        if (pos<a.size || pos<b.size) {
            APPEND_M(str( "MOV `temp, #", (pos<a.size)? a[pos] : "0" ));
            APPEND_M(str( "CMP `temp, #", (pos<b.size)? b[pos] : "0" ));
            APPEND_M(str( "JNE #", early_exit_label ));
        }
    }

    APPEND_M(str( "#:", early_exit_label ));
}

//0 if "a" is zero
void size_limbs(reg_alloc regs, reg_scalar out, fast_asm_integer a) {
    EXPAND_MACROS_SCOPE;

    m.bind(out, "out");

    //1x scalar
    reg_scalar temp=regs.bind_scalar(m, "temp");

    APPEND_M(str( "XOR `out, `out" ));

    string early_exit_label=m.alloc_label();

    const int unroll_factor=4;

    for (int pos=a.size-1;pos>=0;--pos) {
        APPEND_M(str( "MOV `temp, #", to_hex(pos+1) ));
        APPEND_M(str( "CMP QWORD PTR #, 0", a[pos] ));
        APPEND_M(str( "CMOVNZ `out, `temp" ));
        APPEND_M(str( "JNZ #", early_exit_label ));
    }

    APPEND_M(str( "#:", early_exit_label ));
}

//|a|==|b|: out==0
//|a|<|b|: out==-1
//|a|>|b|: out==1
void cmp_abs_1(reg_alloc regs, reg_scalar out, fast_asm_integer a, fast_asm_integer b) {
    EXPAND_MACROS_SCOPE;

    m.bind(out, "out");

    //1x scalar
    cmp_abs(regs, a, b);

    APPEND_M(str( "MOV `out, 0" ));
    APPEND_M(str( "CMOVB `out, #", constant_address_uint64(~0ull, ~0ull) ));
    APPEND_M(str( "CMOVA `out, #", constant_address_uint64(1ull, 1ull) ));
}

//a==b: out==0
//a<b: out==-1
//a>b: out==1
void cmp(reg_alloc regs, reg_scalar out, fast_asm_integer a, fast_asm_integer b) {
    EXPAND_MACROS_SCOPE;

    m.bind(out, "out");

    cmp_abs_1(regs, out, a, b);

    //2x scalar
    reg_scalar temp_0=regs.bind_scalar(m, "temp_0");
    reg_scalar temp_1=regs.bind_scalar(m, "temp_1");

    static bool outputted_table=false;
    if (!outputted_table) {
        #ifdef CHIAOSX
            APPEND_M(str( ".text " ));
        #else
            APPEND_M(str( ".text 1" ));
        #endif

        string pos_1=to_hex(1);
        string pos_0=to_hex(0);
        string neg_1=to_hex(~uint64(0));

        APPEND_M(str( ".balign 8" ));
        APPEND_M(str( "fast_integer_cmp_table:" ));
        //                                      result  A sign      B sign  out
        APPEND_M(str( ".quad #", pos_1 )); //    1      -           -       -1
        APPEND_M(str( ".quad #", pos_0 )); //    0      -           -        0
        APPEND_M(str( ".quad #", neg_1 )); //   -1      -           -        1
        APPEND_M(str( ".quad #", pos_0 )); //    0      -           -      N/A
        APPEND_M(str( ".quad #", neg_1 )); //   -1      -           +       -1
        APPEND_M(str( ".quad #", neg_1 )); //   -1      -           +        0
        APPEND_M(str( ".quad #", neg_1 )); //   -1      -           +        1
        APPEND_M(str( ".quad #", pos_0 )); //    0      -           +      N/A
        APPEND_M(str( ".quad #", pos_1 )); //    1      +           -       -1
        APPEND_M(str( ".quad #", pos_1 )); //    1      +           -        0
        APPEND_M(str( ".quad #", pos_1 )); //    1      +           -        1
        APPEND_M(str( ".quad #", pos_0 )); //    0      +           -      N/A
        APPEND_M(str( ".quad #", neg_1 )); //   -1      +           +       -1
        APPEND_M(str( ".quad #", pos_0 )); //    0      +           +        0
        APPEND_M(str( ".quad #", pos_1 )); //    1      +           +        1
        APPEND_M(str( ".quad #", pos_0 )); //    0      +           +      N/A
        APPEND_M(str( ".text" ));

        outputted_table=true;
    }

    APPEND_M(str( "MOV `temp_0, #", a.sign() ));
    APPEND_M(str( "MOV `temp_1, #", b.sign() ));
    APPEND_M(str( "LEA `temp_0, [`temp_0*2+`temp_1+3]" ));
    APPEND_M(str( "LEA `out, [`temp_0*4+`out+1]" ));

    #ifdef CHIAOSX
        APPEND_M(str( "SHL `out, 3" ));
        APPEND_M(str( "LEA `temp_0, [RIP+fast_integer_cmp_table]" )); //base of the table
        APPEND_M(str( "ADD `out, `temp_0")); //address of the table entry
        APPEND_M(str( "MOV `out, [`out]"));
    #else
        APPEND_M(str( "MOV `out, [fast_integer_cmp_table+`out*8]" ));
    #endif
}

//this clobbers the a and b registers. the data is unchanged
//will XOR b_sign_mask with b's sign
void add(reg_alloc regs, fast_asm_integer out, fast_asm_integer a, fast_asm_integer b, reg_scalar b_sign_mask) {
    EXPAND_MACROS_SCOPE;

    assert(a.padded_size==b.padded_size);
    assert(a.has_sign && b.has_sign && out.has_sign);

    m.bind(a.addr_base, "a");
    m.bind(b.addr_base, "b");
    m.bind(b_sign_mask, "b_sign_mask");

    //3x scalar, 2x vector
    reg_scalar temp_0=regs.bind_scalar(m, "temp_0");
    reg_scalar temp_1=regs.bind_scalar(m, "temp_1");
    reg_vector temp_v0=regs.bind_vector(m, "temp_v0");
    reg_vector temp_v1=regs.bind_vector(m, "temp_v1");

    vector<reg_spill> temp_vs;

    //temp_1=0
    APPEND_M(str( "XOR `temp_1, `temp_1" ));

    cmp_abs(regs, a, b);

    //CF=1 if |a|<|b|
    APPEND_M(str( "MOV `temp_0, `a" ));
    APPEND_M(str( "CMOVB `a, `b" ));
    APPEND_M(str( "CMOVB `b, `temp_0" )); //swap a and b if |a|<|b|

    //out's sign is the same as a's sign after swapping. need to apply b_sign_mask if the inputs were swapped
    APPEND_M(str( "MOV `temp_0, #", a.sign() ));
    APPEND_M(str( "CMOVB `temp_1, `b_sign_mask" )); //temp_1 is 0 if not swapped, b_sign_mask if swapped
    APPEND_M(str( "XOR `temp_0, `temp_1" ));

    //need to add if the signs are the same and subtract if they are different
    APPEND_M(str( "MOV `temp_1, #", a.sign() ));
    APPEND_M(str( "XOR `temp_1, #", b.sign() )); //0 if the signs are the same, -1 if they are different
    APPEND_M(str( "XOR `temp_1, `b_sign_mask" ));
    APPEND_M(str( "MOVQ `temp_v0_128, `temp_1" ));
    APPEND_M(str( "VPBROADCASTQ `temp_v0_256, `temp_v0_128" ));
    APPEND_M(str( "ADD `temp_1, 1" )); //CF and ZF set if the signs are different. CF and ZF cleared if the signs are the same

    APPEND_M(str( "MOV #, `temp_0", out.sign() ));

    int in_size=max(a.size, b.size);
    assert(in_size>=1);

    for (int pos=0;pos<=in_size+1;pos+=4) {
        temp_vs.push_back(regs.get_spill(32, 32));

        APPEND_M(str( "VPXOR `temp_v1_256, `temp_v0_256, #", b[pos] ));
        APPEND_M(str( "VMOVDQU #, `temp_v1_256", temp_vs[pos/4].name() ));
        assert(pos+4<=a.padded_size);
    }

    for (int pos=0;pos<out.size;++pos) {
        int p=min(pos, in_size+1);

        //adding two N-limb integers yields an N+1 limb integer, and all of the limbs after that are the same
        if (p<=in_size+1) {
            APPEND_M(str( "MOV `temp_0, #", (temp_vs[p/4]+(p%4)*8).name() ));
            APPEND_M(str( "ADCX `temp_0, #", a[p] ));
        }
        APPEND_M(str( "MOV #, `temp_0", out[pos] ));
    }
}

//this clobbers the out and b registers. the b data is unchanged
//the arrays can alias but the a register can't be the same as the b or out registers
//this assumes the result will fit into "out". if not the padding for "out" can become nonzero
void mul(reg_alloc regs, fast_asm_integer out, fast_asm_integer a, fast_asm_integer b) {
    EXPAND_MACROS_SCOPE;

    assert(a.has_sign && b.has_sign && out.has_sign);
    assert(a.addr_base.value!=b.addr_base.value && a.addr_base.value!=out.addr_base.value);
    assert(a.size>=1 && b.size>=1);

    m.bind(out.addr_base, "out");
    m.bind(b.addr_base, "b");

    //5x scalar
    reg_scalar temp_0=regs.bind_scalar(m, "temp_0", reg_rdx);

    int num_inc=0;

    //this reduces icache misses if the output is truncated. the sign limb can be overwritten
    const int padding_factor_limbs=4; todo //tune this

    assert(a.padded_size-a.size>=padding_factor_limbs);
    assert(out.padded_size-out.size>=padding_factor_limbs);

    for (int b_pos=0;b_pos<b.size && b_pos<out.size;++b_pos) {
        if (b_pos!=0) {
            APPEND_M(str( "ADD `out, 0x8" ));

            if (b.addr_base.value!=out.addr_base.value) {
                APPEND_M(str( "ADD `b, 0x8" ));
            }

            ++num_inc;
        }

        int truncated_size=out.size-b_pos;
        truncated_size=ceil_div(truncated_size, padding_factor_limbs)*padding_factor_limbs;

        fast_asm_integer c;
        if (b_pos!=0) {
            c=out;
            c.size=min(a.size, truncated_size);
            c.padded_size=c.size;
        }

        fast_asm_integer c_out=out;
        c_out.size=min(a.size+1, truncated_size);
        c_out.padded_size=c_out.size;

        fast_asm_integer c_a=a;
        c_a.size=min(a.size, truncated_size);
        c_a.padded_size=c_a.size;

        //out's sign can't be clobbered because it might alias with a or b's sign
        assert(c_out.size<=out.padded_size-num_inc);

        APPEND_M(str( "MOV `temp_0, #", b[0] ));
        mul_add_call(regs, c_out, c_a, temp_0, c);
        //todo mul_add(regs, c_out, c_a, temp_0, c);
        //todo APPEND_M(str( "NOP" ));
    }

    bool assigned_zero=false;
    for (int pos=min(a.size+b.size, out.size);pos<=out.size;++pos) {
        if (!assigned_zero && pos<out.size) {
            APPEND_M(str( "XOR `temp_0, `temp_0" ));
            assigned_zero=true;
        }

        if (pos==out.size) {
            //the output sign is the XOR of the input signs
            APPEND_M(str( "MOV `temp_0, #", a.sign() ));
            APPEND_M(str( "XOR `temp_0, #", b[b.padded_size - num_inc] ));
        }

        APPEND_M(str( "MOV #, `temp_0", out[((pos<out.size)? pos : out.padded_size) - num_inc] ));
    }
}

//amount is between 0 and 64 inclusive
void shr(reg_alloc regs, fast_asm_integer out, reg_scalar amount, fast_asm_integer a) {
    EXPAND_MACROS_SCOPE;

    assert(out.has_sign && a.has_sign);

    m.bind(amount, "amount");

    //1x scalar, 4x vector
    reg_scalar temp_0=regs.bind_scalar(m, "temp_0");
    reg_vector current=regs.bind_vector(m, "current");
    reg_vector next=regs.bind_vector(m, "next");
    reg_vector shr_amount=regs.bind_vector(m, "shr_amount");
    reg_vector shl_amount=regs.bind_vector(m, "shl_amount");

    APPEND_M(str( "MOVQ `shr_amount, `amount" ));
    APPEND_M(str( "VPBROADCASTQ `shr_amount, `shr_amount_128" ));

    APPEND_M(str( "MOV `temp_0, 64" ));
    APPEND_M(str( "SUB `temp_0, `amount" ));
    APPEND_M(str( "MOVQ `shl_amount, `temp_0" ));
    APPEND_M(str( "VPBROADCASTQ `shl_amount, `shl_amount_128" ));

    for (int pos=0;pos<a.size;pos+=4) {
        APPEND_M(str( "VMOVDQU `current, #", a[pos] ));
        APPEND_M(str( "VMOVDQU `next, #", a[pos+1] ));
        assert(pos+1+4<=a.padded_size+1);

        APPEND_M(str( "VPSRLQ `current, `current, `shr_amount_128" ));
        APPEND_M(str( "VPSLLQ `next, `next, `shl_amount_128" ));
        APPEND_M(str( "VPOR `current, `current, `next" ));

        APPEND_M(str( "VMOVDQU #, `current", out[pos] ));
        assert(pos+4<=out.padded_size); //out's sign can't be clobbered because it might alias with a's sign
    }

    if (out.addr_base.value!=a.addr_base.value) {
        APPEND_M(str( "MOV `temp_0, #", a.sign() ));
        APPEND_M(str( "MOV #, `temp_0", out.sign() ));
    }
}

//modulo 2^64. this is the same as binvert_limb in GMP. in must be odd
void modular_inverse(reg_alloc regs, reg_scalar in, reg_scalar out) {
    EXPAND_MACROS_SCOPE;

    m.bind(in, "in");
    m.bind(out, "out");

    //2x scalar (1x if in and out are different)
    reg_scalar temp_0;
    if (in.value==out.value) {
        temp_0=regs.bind_scalar(m, "temp_0");
    } else {
        temp_0=out;
        m.bind(temp_0, "temp_0");
    }
    reg_scalar temp_1=regs.bind_scalar(m, "temp_1");

    static bool outputted_table=false;
    if (!outputted_table) {
        #ifdef CHIAOSX
            APPEND_M(str( ".text " ));
        #else
            APPEND_M(str( ".text 1" ));
        #endif

        //each table entry is v such that (2*i+1)*v === 1 mod 256
        APPEND_M(str( "fast_integer_modular_inverse_table:" ));

        for (int i=0;i<128;++i) {
            int v=-1;
            for (int x=0;x<256;++x) {
                if ((((2*i+1)*x) & 0xFF) == 1) {
                    assert(v==-1);
                    v=x;
                }
            }
            assert(v!=-1);

            APPEND_M(str( ".byte #", to_hex(v) ));
        }

        APPEND_M(str( ".text" ));

        outputted_table=true;
    }

    //temp_0 = fast_integer_modular_inverse_table[(in >> 1) & 0x7F]
    APPEND_M(str( "XOR `temp_0, `temp_0" ));
    APPEND_M(str( "MOV `temp_0_8, `in" ));
    APPEND_M(str( "SHR `temp_0_8, 1" ));
    APPEND_M(str( "MOV `temp_0_8, [fast_integer_modular_inverse_table+`temp_0_8]" ));

    for (int x=0;x<3;++x) {
        //temp_0 = 2*temp_0 - temp_0*temp_0*in
        APPEND_M(str( "MOV `temp_1, `temp_0" ));
        APPEND_M(str( "IMUL `temp_1, `temp_1" ));
        APPEND_M(str( "IMUL `temp_1, `in" )); // temp_1 = temp_0*temp_0*in

        APPEND_M(str( "ADD `temp_0, `temp_0" ));
        APPEND_M(str( "SUB `temp_0, `temp_1" )); // temp_0 = 2*temp_0 - temp_1
    }

    if (out.value!=temp_0.value) {
        APPEND_M(str( "MOV `out, `temp_0" ));
    }
}

/*void div_exact_shift_right_until_odd() {
}

void div_exact(
    reg_alloc regs, fast_asm_integer out, fast_asm_integer a_buffer, fast_asm_integer b_buffer, fast_asm_integer a, fast_asm_integer b
) {


    -calculate modular inverse of the low digit of the divisor. this doesnt work if the low digit is 0
    --the divisor has to be odd for this algorithm to work
    --if the divisor is even then both integers have to be right shifted until it is odd
    --the right shift uses lzcnt to decide how much to shift by (up to 64) then does the right shift. will continue right shifting
    -- if the result is odd or a is 0
    -- will just have the algorithm fail if a single right shift doesnt make it odd
    -- both a and b are right shifted by the same amount. a must have at least as many leading zeros as b
    -- dont shift if the shift amount is 0, which will be true half the time
    -figure out how many quotient limbs to calculate. all following quotient limbs are 0
    --this depends on the number of nonzero limbs in a and b. it can be up to as size
    --this needs a jump table
    -truncate a to the number of quotient limbs being calculated by removing the high limbs. can be 0 if there is 1 quotient limb
    -calculate the quotient limb
    -scale b by the quotient limb and subtract the result from a. the bottom limb of a will be 0
    -truncate off the last limb of a
} */


}}