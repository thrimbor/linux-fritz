
/*
 * Format of an instruction in memory.
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 2000 by Ralf Baechle
 * Copyright (C) 2006 by Thiemo Seufer
 */
#ifndef _ASM_INST_MIPS16_H
#define _ASM_INST_MIPS16_H

/*------------------------------------------------------------------------------------------*\
 * Binary-Register-Coding:   0 and 1 ==> 16, 17
\*------------------------------------------------------------------------------------------*/
#define MIPS16_REG(REG) (((REG) < 2) ? ((REG) | 0x10) : (REG))

#define MIPS16_INSN_SIZE(OPCODE) (((OPCODE == mips16_op_ext) || (OPCODE == mips16_op_jal)) ? 4 : 2)


enum mips16_major_op {
    mips16_op_addiusp       = 0x00,
	mips16_op_addiupc       = 0x01,
	mips16_op_b             = 0x02,
	mips16_op_jal           = 0x03,
	mips16_op_beqz          = 0x04,
	mips16_op_bnez          = 0x05,
	mips16_op_shift         = 0x06,
    mips16_op_high_level_0  = 0x07,
	mips16_op_rri_a         = 0x08,
	mips16_op_addiu8        = 0x09,
	mips16_op_slti          = 0x0a,
	mips16_op_sltiu         = 0x0b,
	mips16_op_i8            = 0x0c,
	mips16_op_li            = 0x0d,
	mips16_op_cmpi          = 0x0e,
    mips16_op_high_level_1  = 0x0f,
	mips16_op_lb            = 0x10,
	mips16_op_lh            = 0x11,
	mips16_op_lwsp          = 0x12,
	mips16_op_lw            = 0x13,
	mips16_op_lbu           = 0x14,
	mips16_op_lhu           = 0x15,
	mips16_op_lwpc          = 0x16,
    mips16_op_high_level_2  = 0x17,
	mips16_op_sb            = 0x18,
	mips16_op_sh            = 0x19,
	mips16_op_swsp          = 0x1a,
	mips16_op_sw            = 0x1b,
	mips16_op_rrr           = 0x1c,
	mips16_op_rr            = 0x1d,
	mips16_op_ext           = 0x1e,
    mips16_op_high_level_3  = 0x1f
};

enum mips16_i8_func {
    mips16_i8_bteqz  = 0x0,
    mips16_i8_btnez  = 0x1,
    mips16_i8_swrasp = 0x2,
    mips16_i8_adjsp  = 0x3,
    mips16_i8_svrs   = 0x4,
    mips16_i8_mov32r = 0x5,
    mips16_i8_movr32 = 0x7
};

enum mips16_rr_func {
    mips16_rr_jr    = 0x00,
    mips16_rr_sdbbp = 0x01,
    mips16_rr_slt   = 0x02,
    mips16_rr_sltu  = 0x03,
    mips16_rr_sllv  = 0x04,
    mips16_rr_break = 0x05,
    mips16_rr_srlv  = 0x06,
    mips16_rr_srav  = 0x07,
    mips16_rr_cmp   = 0x0a,
    mips16_rr_neg   = 0x0b,
    mips16_rr_and   = 0x0c,
    mips16_rr_or    = 0x0d,
    mips16_rr_xor   = 0x0e,
    mips16_rr_not   = 0x0f,
    mips16_rr_mfhi  = 0x10,
    mips16_rr_cnvt  = 0x11,
    mips16_rr_mflo  = 0x12,
    mips16_rr_mult  = 0x18,
    mips16_rr_multu = 0x19,
    mips16_rr_div   = 0x1a,
    mips16_rr_divu  = 0x1b
};

enum mips16_jr_func {
    mips16_jr_jr_rx  = 0x0,
    mips16_jr_jr_ra  = 0x1,
    mips16_jr_jalr   = 0x2,
    mips16_jr_jrc_rx = 0x4,
    mips16_jr_jrc_ra = 0x5,
    mips16_jr_jalrc  = 0x6
};


struct mips16_general_format {
	unsigned int opcode : 5;
	unsigned int params : 11;
};

struct mips16_i_format {	/* format with given rx */
	unsigned int opcode : 5;
    unsigned int offset : 11;
};

struct mips16_ri_format {	/* format with given rx */
	unsigned int opcode : 5;
	unsigned int rx     : 3;
    unsigned int offset : 8;
};

struct mips16_rr_format {	/* format with given rx & ry */
	unsigned int opcode : 5;
	unsigned int rx     : 3;
    unsigned int ry     : 3;
    unsigned int offset : 5;
};

struct mips16_ext_rr_format {	/* lw/sw format with given rx & ry */
    unsigned int op_ext  : 5;
    unsigned int offset1 : 6;
    unsigned int offset2 : 5;
	unsigned int opcode  : 5;
	unsigned int rx      : 3;
    unsigned int ry      : 3;
    unsigned int offset0 : 5;
};

struct mips16_j_format {	/* jump format */
	unsigned int opcode   : 5;
	unsigned int x_mips16 : 1;
	unsigned int target1  : 5;
	unsigned int target2  : 5;
	unsigned int target0  : 16;
};

#if 0
union mips16_instruction {
    /*--- XXX ---*/
	unsigned short halfword[1];
	unsigned char byte[2];
	/*--- struct j_format j_format; ---*/
	struct i_format i_format;
	/*--- struct u_format u_format; ---*/
	/*--- struct c_format c_format; ---*/
	/*--- struct r_format r_format; ---*/
	/*--- struct f_format f_format; ---*/
    /*--- struct ma_format ma_format; ---*/
    /*--- struct sp3_format sp3_format; ---*/
    /*--- struct lx_format lx_format; ---*/
};
#endif

union mips16_instruction {
    struct _insn_meta_data {
	    unsigned int word;
        unsigned int is_extended;
        enum mips16_major_op opcode;
    } meta;
	unsigned int word;
	unsigned short halfword[2];
	unsigned char byte[4];
	struct mips16_general_format gen_format;
	struct mips16_i_format       i_format;
	struct mips16_ri_format      ri_format;
	struct mips16_rr_format      rr_format;
	struct mips16_ext_rr_format  ext_rr_format;
	struct mips16_j_format       j_format;
};




void emulate_load_store_insn_mips16(struct pt_regs *regs, void __user *addr, unsigned int __user *pc);
int __compute_return_epc_mips16(struct pt_regs *regs);



/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void get_mips16_offset(union mips16_instruction *insn, short *poffset)
{
    if(insn->gen_format.opcode == mips16_op_ext) {
        *poffset = (short)( insn->ext_rr_format.offset0       |
                           (insn->ext_rr_format.offset1 << 5) |
                           (insn->ext_rr_format.offset2 << 11) );
    } else {
        *poffset = insn->i_format.offset;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void get_mips16_rx(union mips16_instruction *insn, short *prx, short *poffset, unsigned int shift)
{
    if(insn->gen_format.opcode == mips16_op_ext) {
        *prx = MIPS16_REG(insn->ext_rr_format.rx);
        *poffset = (short)( insn->ext_rr_format.offset0       |
                           (insn->ext_rr_format.offset1 << 5) |
                           (insn->ext_rr_format.offset2 << 11) );
    } else {
        *prx = MIPS16_REG(insn->ri_format.rx);
        *poffset = insn->ri_format.offset << shift;
    }
}

/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline void get_mips16_rxy(union mips16_instruction *insn, short *prx, short *pry, short *poffset, unsigned int shift)
{
    if(insn->gen_format.opcode == mips16_op_ext) {
        *prx = MIPS16_REG(insn->ext_rr_format.rx);
        *pry = MIPS16_REG(insn->ext_rr_format.ry);
        *poffset = (short)( insn->ext_rr_format.offset0       |
                           (insn->ext_rr_format.offset1 << 5) |
                           (insn->ext_rr_format.offset2 << 11) );
    } else {
        *prx = MIPS16_REG(insn->rr_format.rx);
        *pry = MIPS16_REG(insn->rr_format.ry);
        *poffset = insn->rr_format.offset << shift;
    }
}


/*------------------------------------------------------------------------------------------*\
\*------------------------------------------------------------------------------------------*/
static inline unsigned int compute_mips16_insn_size(unsigned int epc) 
{
    union mips16_instruction insn;
    unsigned short *mips16_epc = (unsigned short __user *)(epc & ~0x1);

    __get_user(insn.halfword[0], mips16_epc);
#if 0
    printk(KERN_ERR "[%s] mips16_epc=0x%x %sopcode=0x%x op_size=%d\n", __FUNCTION__, (unsigned int)mips16_epc, 
            insn.gen_format.opcode == mips16_op_ext ? "extended_" : "", (unsigned int)insn.gen_format.opcode,
            MIPS16_INSN_SIZE(insn.gen_format.opcode));

#endif
    return MIPS16_INSN_SIZE(insn.gen_format.opcode);
}

/*------------------------------------------------------------------------------------------*\
 * Read the instruction (asuming big endian)
\*------------------------------------------------------------------------------------------*/
static inline int read_mips16_insn(union mips16_instruction *insn, unsigned int pc)
{
    unsigned short __user *mips16_pc = (unsigned short __user *)((int)pc & ~0x1);

    memset(insn, 0, sizeof(union mips16_instruction));
	if(__get_user(insn->halfword[0], mips16_pc)) {
        return -1;
    }
    insn->meta.opcode = insn->gen_format.opcode;
    if(MIPS16_INSN_SIZE(insn->meta.opcode) == 4) {
	    if(__get_user(insn->halfword[1], mips16_pc + 1))
            return -1;
        if(insn->meta.opcode == mips16_op_ext) {
            insn->meta.opcode      = insn->ext_rr_format.opcode;
            insn->meta.is_extended = 1;
        }
    }
    return 0;
}

#endif /*--- #ifndef _ASM_INST_MIPS16_H ---*/

