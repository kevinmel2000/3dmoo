/*
* Copyright (C) 2014 - ichfly
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "util.h"
#include "handles.h"
#include "mem.h"
#include "arm11.h"
#include "dsp.h"

#define DISASM 1

u8 ram[0x20000];

//register
u16 pc = 0;
u16 sp = 0;
u16 r[6];
u16 rb = 0;
u16 y = 0;
u16 st[3];
u16 cfgi = 0;
u16 cfgj = 0;
u16 ph = 0;
u16 b[2];
u16 a[2];
u16 ext[4];
u16 sv = 0;
u32 onec = 0; //wtf is that I don't know


static u32 Read32(uint8_t p[4])
{
    u32 temp = p[0] | p[1] << 8 | p[2] << 16 | p[3] << 24;
    return temp;
}

static u16 FetchWord(u16 addr)
{
    u16 temp = ram[addr*2] | (ram[addr*2 + 1] << 8);
    return temp;
}

#define Disarm 1
#define emulate 1

/*
inter RESET 0x0
inter TRAP/BI 0x2
inter NMI 0x4
inter INT0 0x6
inter INT1 0xE
inter INT2 0x16
*/
const char* mulXXX[] = {
    "mpy", "mpysu", "mac", "macus",
    "maa", "macuu", "macsu", "maasu"
};
const char* mulXX[] = {
    "mpy", "mac",
    "maa", "macsu"
};

const char* ops[] = {
    "or", "and", "xor", "add", "tst0_a", "tst1_a",
    "cmp", "sub", "msu", "addh", "addl", "subh", "subl",
    "sqr", "sqra", "cmpu"
};

const char* ops3[] = {
    "or", "and", "xor", "add",
    "invalid!", "invalid!", "cmp", "sub"
};

const char* alb_ops[] = {
    "set", "rst", "chng", "addv", "tst0_mask1", "tst0_mask2", "cmpv", "subv"
};

const char* mm[] = {
    "nothing", "+1", "-1", "+step"
};
const char* rrrrr[] = {
    "r0",
    "r1",
    "r2",
    "r3",
    "r4",
    "r5",
    "rb",
    "y",
    "st0",
    "st1",
    "st2",
    "p / ph",
    "pc",
    "sp",
    "cfgi",
    "cfgj",
    "b0h",
    "b1h",
    "b01",
    "b1l",
    "ext 0",
    "ext1",
    "ext2",
    "ext3",
    "a0",
    "a1",
    "a0l",
    "a1l",
    "a0h",
    "a1h",
    "1c",
    "sv"
}; 

const char* AB[] = {
    "b0", "b1", "a0", "a1"
};

const char* cccc[] = {
    "true", "eq", "neg", "gt",
    "ge", "1t", "le", "nn",
    "c", "v", "e", "l",
    "nr", "niu0", "iu0", "iu1"
};

const char* fff[] = {
    "shr", "shr4", "shl", "shl4",
    "ror", "rol", "clr", "reserved"

};
const char* rNstar[] = {
    "r0", "r1", "r2", "r3",
    "r4", "r5", "rb", "y"
};
const char* ABL[] = {
    "B0L", "B0H", "B1L", "B1H", "A0L", "A0H", "A1L", "A1H"
};

const char* swap[] = {
    "a0 <-> b0",
    "a0 <-> b1",
    "a1 <-> b0",
    "a1 <-> b1",
    "a0 <-> b0 and a1 -> b1",
    "a0 <-> b1 and a1 -> b0",
    "a0 -> b0 and -> a1",
    "a0 -> b1 -> a1",
    "a1 -> b0 -> a0",
    "a1 -> b0 ->a0",
    "b0 -> a1 -> b1",
    "b0 -> a1 -> b1",
    "b1 -> a0 -> b0",
    "b1 -> a1 -> b0",
    "unk.",
    "unk."
}; 

const char* ffff[] = {
    "shr",
    "shr4",
    "shl",
    "shl4",
    "ror",
    "rol",
    "clr",
    "reserved",
    "not",
    "neg",
    "rnd",
    "pacr",
    "clrr",
    "inc",
    "dec",
    "copy"
};

int HasOp3(u16 op)
{
    u16 opc = (op >> 9) & 0x7;

    if(opc != 4 && opc != 5)
        return opc;

    return -1;
}

bool cccccheck(u8 cccc)
{
    switch (cccc)
    {
    case 0:
        return true;
    default:
        DEBUG("unk. cccc %d",cccc);
        return true;
    }
}
u16 fixending(u16 in, u8 pos)
{
    if (in & (1 << (pos - 1)))
    {
        return (in | (0xFFFF << (pos - 1)));
    }
    return in;
}
void DSP_Step()
{
    // Currently a disassembler.
    // Implemented up to MUL y, (rN).

    u16 op = FetchWord(pc);
    u8 ax = op & (0x100) ? 1 : 0; // use a0 or a1

    switch(op >> 12) {
    case 0:
        if ((op&~0x7) == 0x30)//hope that is correct
        {
            u16 extra1 = FetchWord(pc + 1);
            DEBUG("mov #0x%04x,a%d\n", extra1, (op>>2)&0x1);
            a[(op >> 2) & 0x1] = extra1;
            pc++;
            break;
        }
        if (op == 0x20)
        {
            DEBUG("trap\n");
            break;
        }
        if (op == 0)
        {
            DEBUG("nop\n");
            break;
        }
        if ((op & 0xE00) == 0x200)
        {
            DEBUG("load modi #%04x\n", op & 0x1FF);
            break;
        }
        if ((op & 0xE00) == 0xA00)
        {
            DEBUG("load modj #%04x\n", op & 0x1FF);
            break;
        }
        if ((op & 0xF00) == 0x400)
        {
            DEBUG("load page #%02x\n", op & 0xFF);
            st[1] = (st[1] & 0xFF00) | (op & 0xFF);
            break;
        }
        if ((op & 0xF80) == 0x80)
        {
            DEBUG("rets (r%d) (modifier=%s) (disable=%d)\n", op & 0x7, mm[(op >> 3) & 3],(op>>5)&0x1);
            break;
        }
        if ((op & 0xF00) == 0x900)
        {
            DEBUG("rets #%02x\n", op&0xFF);
            break;
        }
        if ((op & 0xF80) == 0x100)
        {
            DEBUG("movs %s, %s\n",rrrrr[op&0x1F],AB[(op >> 5)&0x3]);
            break;
        }
        if ((op & 0xF80) == 0x180)
        {
            DEBUG("movs (r%d) (modifier=%s), %s\n", op&0x7,mm[(op>>3)&3], AB[(op >> 5) & 0x3]);
            break;
        }
        if ((op & 0xE00) == 0x600)
        {
            DEBUG("movp (r%d) (modifier=%s), (r%d) (modifier=%s) %s\n", op & 0x7, mm[(op >> 3) & 3],(op>>5)&0x3,mm[(op>>7)&0x3]);
            break;
        }
        if ((op & 0xFC0) == 0x040)
        {
            DEBUG("movp a%d, %s\n", (op >> 5) & 0x1, rrrrr[op & 0x1F]);
            break;
        }
        if ((op & 0xFC0) == 0x040)
        {
            DEBUG("movp a%d, %s\n",(op >> 5) &0x1, rrrrr[op&0x1F]);
            break;
        }
        if((op&0xF00) == 0x800)
        {
            DEBUG("mpyi %02X\n", op & 0xFF);
            break;
        }
        if((op & 0xE00) == 0xE00) {
            // TODO: divs
            DEBUG("divs??\n");
            break;
        }
        if ((op & 0xF00) == 0x500)
        {
            DEBUG("mov %02x, sv\n",op&0xFF);
            break;
        }
        if ((op & 0xF00) == 0xD00) //00001101...rrrrr
        {
            DEBUG("rep %s\n", rrrrr[op & 0x1F]);
            break;
        }
        if ((op & 0xF00) == 0xC00)
        {
            DEBUG("rep %02x\n", op&0xFF);
            break;
        }
        DEBUG("? %04X\n", op);
        break;
    case 1:
        if (!(op & 0x800))
        {
            DEBUG("callr %s %02x\n", cccc[op & 0xF], (op >> 4) & 0x7F);
            break;
        }
        if ((op & 0xC00) == 0x800)
        {
            DEBUG("mov %s, (r%d) (modifier=%s)\n",rrrrr[(op >> 5)&0x1F], op & 0x7, mm[(op >> 3) & 3]);
            break;
        }
        if ((op & 0xC00) == 0xC00)
        {
            DEBUG("mov (r%d) (modifier=%s), %s\n", op & 0x7, mm[(op >> 3) & 3], rrrrr[(op >> 5) & 0x1F]);
            break;
        }
        DEBUG("? %04X\n", op);
        break;
    case 2:
        if (!(op & 0x100))
        {
            DEBUG("mov %s, #%02x\n", rNstar[(op >> 9) & 0x7],op&0xFF);
            break;
        }
    case 3:
        if((op&0xF100) ==0x3000)
        {
            DEBUG("mov %s, #%02x\n",ABL[(op >>9)&0x7],op&0xFF);
            break;
        }
        if ((op & 0xF00) == 0x100)
        {
            DEBUG("mov #%02x, a%dl\n", (op>>12)&0x1, op & 0xFF);
            break;
        }
        if ((op & 0xF00) == 0x500)
        {
            DEBUG("mov #%02x, a%dh\n", (op >> 12) & 0x1, op & 0xFF);
            break;
        }
        if ((op & 0x300) == 0x300)
        {
            DEBUG("mov #%02x, %s\n", op & 0xFF,rNstar[(op>>10)&0x7]);
            break;
        }
        if ((op & 0xB00) == 0x900)
        {
            DEBUG("mov #%02x, ext%d\n", op & 0xFF, ((op>>11)&0x2) | ((op >>10) &0x1));
            break;
        }

        DEBUG("? %04X\n", op);
        break;
    case 4:

        if(!(op & 0x80)) {
            int op3 = HasOp3(op);

            if(op3 != -1) {
                // ALU (rb + #offset7), ax
                DEBUG("%s (rb + %02x), a%d\n", ops3[op3], op & 0x7F, ax);
                break;
            }
        }
        if (op == 0x43C0)
        {
            DEBUG("dint\n");
            break;
        }
        if (op == 0x4380)
        {
            DEBUG("eint\n");
            break;
        }
        if ((op & ~0x3) == 0x4D80)
        {
            DEBUG("load ps %d\n", op&0x3);
            break;
        }
        if ((op & 0xFE0) == 0x5C0)
        {
            DEBUG("reti %s (switch=%d)\n", cccc[op & 0xF],(op>>4)&1);
            break;
        }
        if ((op & 0xFF0) == 0x580)
        {
            DEBUG("ret %s \n", cccc[op&0xF]);
            break;
        }
        if ((op & ~0xF00F) == 0x180)
        {
            u16 extra = FetchWord(pc + 1);
#ifdef Disarm
            DEBUG("br %s %04x\n", cccc[op&0xF],extra);
#endif
            pc++;
#ifdef emulate
            if (cccccheck(op&0xF))pc = extra - 1;
#endif
            break;
        }
        if ((op&~0x7F) == 0x4B80)
        {
            DEBUG("banke #%02x\n", op & 0x7F);
            break;
        }
        if ((op&~0x3F) == 0x4980)
        {
            DEBUG("swap %s\n", swap[op&0xF]);
            break;
        }
        if ((op & 0xC0) == 0x40) //0100111111-rrrrr
        {
            DEBUG("movsi %s, %s (#%02x)\n", rNstar[(op >> 9)&0x7], AB[(op >>5)&0x3],op&0x1F);
            break;
        }
        if ((op & 0xFC0) == 0xFC0) //0100111111-rrrrr
        {
            DEBUG("mov %s, icr\n", rrrrr[op&0x1F]);
            break;
        }
        if ((op & 0xFC0) == 0xF80) //0100111111-vvvvv
        {
            DEBUG("mov %d, icr\n", op & 0x1F);
            break;
        }
        if((op & 0xFFFC) == 0x421C) {
            // lim
            switch(op & 3) {
            case 0:
                DEBUG("lim a0\n");
                break;
            case 1:
                DEBUG("lim a0, a1\n");
                break;
            case 2:
                DEBUG("lim a1, a0\n");
                break;
            case 3:
                DEBUG("lim a1\n");
                break;
            }

            break;
        }
        if((op & 0xFE0) == 0x7C0)
        {
            DEBUG("mov mixp , %s\n", rrrrr[op & 0x1F]);
            break;
        }
        if ((op & 0xFE0) == 0x7E0)
        {
            DEBUG("mov sp, %s\n", rrrrr[op & 0x1F]);
            break;
        }
        if ((op&0xFF0) == 0x1C0)
        {
            u16 extra = FetchWord(pc + 1);
            DEBUG("call %s %04x\n", cccc[op & 0xF], extra);
            pc++;
            break;
        }
        DEBUG("? %04X\n", op);
        break;
    case 0x5:
        if (!(op & 0x800))
        {
            DEBUG("brr %s %02x\n", cccc[op & 0xF], (op >> 4)&0x7F);
            break;
        }
        if ((op & 0xF00) == 0xC00)
        {
            DEBUG("bkrep %02x\n", op & 0xFF);
            break;
        }
        if ((op & ~0x1F) == 0xD00)
        {
            DEBUG("bkrep %s\n", rrrrr[op&0x1F]);
            break;
        }
        if ((op &0xF00) == 0xF00)
        {
            DEBUG("movd r%d (modifier=%s),r%d (modifier=%s)\n",3 + (op >> 2) & 0x1, mm[(op >> 3) & 0x3] ,op & 0x3, mm[(op >> 5) & 0x3]);
            break;
        }
        if ((op &~0x1F) == 0x5E60)
        {
            DEBUG("pop %s\n", rrrrr[op & 0x1F]);
            break;
        }
        if (op == 0x5F40)
        {
            u16 extra = FetchWord(pc + 1);
            DEBUG("push #%04x\n", extra);
            pc++;
            break;
        }
        if ((op & 0xFE0) == 0xE40)
        {
            DEBUG("push %s\n", rrrrr[op&0x1F]);
            break;
        }
        if ((op & 0xEE0) == 0xE00) //0101111-000rrrrr
        {
            u16 extra = FetchWord(pc + 1);
            DEBUG("mov #%04x, %s\n", extra, rrrrr[op&0x1F]);
            pc++;
            break;
        }
        if ((op & 0xEE0) == 0xEE0) //0101111b001-----
        {
            u16 extra = FetchWord(pc + 1);
            DEBUG("mov #%04x, b%d\n", extra, ax);
            pc++;
            break;
        }
        if ((op & 0xC00) == 0x800)
        {
            DEBUG("mov %s, %s\n",rrrrr[op & 0x1F] ,rrrrr[(op >> 5) & 0x1F] );
            break;
        }
        if ((op & 0xFFC0) == 0x5EC0)
        {
            DEBUG("mov %s, b%d\n", rrrrr[op& 0x1F],(op>>5)&0x1);
            break;
        }
        if ((op & 0xFFE0) == 0x5F40)
        {
            DEBUG("mov %s, mixp\n", rrrrr[op & 0x1F]);
            break;
        }
        DEBUG("? %04X\n", op);
        break;
    case 0x6:
    case 0x7:
        if ((op & 0xF00) == 0x700)
        {
            DEBUG("%s %s a%d\n", ffff[(op>> 4)&0xF],cccc[op&0xF],(op>>12)&0x1);
            break;
        }
        if ((op&0x700) == 0x300)
        {
            DEBUG("movs %02x, %s\n",op&0xFF,AB[(op>> 11)&0x3]);
            break;
        }
        if ((op & 0xEF80) == 0x6F00)
        {
            DEBUG("%s b%d Bit %d\n",fff[(op >> 4)&0x7],(op >> 12)&0x1,op&0xF);
            break;
        }
        if ((op & 0xE300) == 0x6000)
        {
            DEBUG("mov #%02x ,%s\n",op&0xFF ,rNstar[(op >> 10)&0x7]);
            break;
        }
        if ((op & 0xE700) == 0x6100)
        {
            DEBUG("mov #%02x ,%s\n", op & 0xFF, AB[(op >> 11) & 0x3]);
            break;
        }
        if ((op & 0xE300) == 0x6200)
        {
            DEBUG("mov #%02x ,%s\n", op & 0xFF, ABL[(op >> 10) & 0x7]);
            break;
        }
        if ((op & 0xEF00) == 0x6500)
        {
            DEBUG("mov #%02x ,a%dHeu\n", op & 0xFF, (op >> 12)&0x1);
            break;
        }
        if ((op & 0xFF00) == 0x6D00)
        {
            DEBUG("mov #%02x ,sv\n", op & 0xFF);
            break;
        }
        if ((op & 0xFF00) == 0x7D00)
        {
            DEBUG("mov sv ,#%02x\n", op & 0xFF);
            break;
        }
        DEBUG("? %04X\n", op);
        break;
    case 0x8:
        if ((op & 0xE0) == 0x60) {
            //MUL y, (rN)
            DEBUG("%s y, (a%d),(r%d) (modifier=%s)\n", mulXXX[(op >> 8) & 0x7], (op >> 11) & 0x1, op&0x7, mm[(op >> 3) & 3]);
            break;
        }
        if ((op & 0xE0) == 0x40) {
            //MUL y, register
            DEBUG("%s y, (a%d),%s\n", mulXXX[(op >> 8) & 0x7], (op >> 11) & 0x1, rrrrr[op & 0x1F]);
            break;
        }
        if ((op & 0xE0) == 0x00) {
            //MUL (rN), ##long immediate
            u16 longim = FetchWord(pc + 1);
            DEBUG("%s %s, (a%d),%04x\n", mulXXX[(op >> 8) & 0x7], rrrrr[op & 0x1F], (op >> 11) & 0x1, longim);
            pc++;
            break;
        }
    case 0x9:
        if ((op & 0xE0) == 0xA0)
        {
            DEBUG("%s a%dl ,%s\n", ops3[(op >>9)&0xF], ax, rrrrr[op&0x1F]);
            break;
        }
        if ((op & 0xF240) == 0x9240)
        {
            DEBUG("shfi %s ,%s %02x\n", AB[(op >> 10) & 0x3], AB[(op >> 7) & 0x3], fixending(op & 0x3F,6));
            break;
        }

        if ((op & 0xFEE0) == 0x9CC0)
        {
            DEBUG("movr %s ,a%d\n", rrrrr[op&0x1F], ax);
            break;
        }
        if ((op & 0xFEE0) == 0x9CC0)
        {
            DEBUG("mov (r%d) (modifier=%s) ,a%d\n", op & 0x7, mm[(op >> 3) & 3], ax);
            break;
        }
        if ((op & 0xFEE0) == 0x98C0)
        {
            DEBUG("mov (r%d) (modifier=%s) ,b%d\n", op & 0x7, mm[(op >> 3) & 3], ax);
            break;
        }
        if ((op & 0xFEE0) == 0x9840)
        {
            DEBUG("exp r%d (modifier=%s), a%d\n", op & 0x7, mm[(op >> 3) & 3], ax);
            break;
        }
        if ((op & 0xFEE0) == 0x9040)
        {
            DEBUG("exp %s, a%d\n", rrrrr[op & 0x1F], ax);
            break;
        }
        if ((op & 0xFEFE) == 0x9060)
        {
            DEBUG("exp b%d, a%d\n", op & 0x1, ax);
            break;
        }
        if ((op & 0xFEFE) == 0x9C40)
        {
            DEBUG("exp r%d (modifier=%s), sv\n", op & 0x7, mm[(op >> 3) & 3]);
            break;
        }
        if ((op & 0xFEFE) == 0x9440)
        {
            DEBUG("exp %s, sv\n", rrrrr[op & 0x1F]);
            break;
        }
        if ((op & 0xFFFE) == 0x9460)
        {
            DEBUG("exp b%d, sv\n", op & 0x1);
            break;
        }

        if((op & 0xFEE0) == 0x90C0)
        {
            u16 extra = FetchWord(pc + 1);
            DEBUG("msu (a%d) (r%d), %04x (modifier=%s)\n",ax, op & 0x7,extra, mm[(op >> 3) & 3]);
            pc++;
            break;
        }
        if((op&0xF0E0) == 0x9020)
        {
            DEBUG("tstb (r%d) (modifier=%s) (bit=%d)\n", op & 0x7, mm[(op >> 3) & 3],(op >> 8)&0xF);
            break;
        }
        if ((op & 0xF0E0) == 0x9000)
        {
            DEBUG("tstb %s (bit=%d)\n", rrrrr[op&0x1F], (op >> 8) & 0xF);
            break;
        }
        if(((op >> 6) & 0x7) == 4) {
            // ALM (rN)
            DEBUG("%s (r%d), a%d (modifier=%s)\n", ops[(op >> 9) & 0xF], op & 0x7, ax, mm[(op >> 3) & 3]);
            break;
        } else if(((op >> 6) & 0x7) == 5) {
            // ALM register
            u16 r = op & 0x1F;

            if(r < 22) {
                DEBUG("%s %s, a%d\n", ops[(op >> 9) & 0xF], rrrrr[r], ax);
                break;
            }

            DEBUG("? %04X\n", op);
        } else if(((op >> 6) & 0x7) == 7) {
            u16 extra = FetchWord(pc+2);
            pc++;

            if(!(op & 0x100)) {
                // ALB (rN)
                DEBUG("%s (r%d), %04x (modifier=%s)\n", alb_ops[(op >> 9) & 0x7], op & 0x7,
                      extra & 0xFFFF, mm[(op >> 3) & 3]);
                break;
            } else {
                // ALB register
                u16 r = op & 0x1F;

                if(r < 22) {
                    DEBUG("%s %s, %04x\n", alb_ops[(op >> 9) & 0x7], rrrrr[r], extra & 0xFFFF);
                    break;
                }

                DEBUG("? %04X\n", op);
            }
            break;
        } else if((op & 0xF0FF) == 0x80C0) {
            int op3 = HasOp3(op);

            if(op3 != -1) {
                // ALU ##long immediate
                u16 extra = FetchWord(pc+1);
                pc++;

                DEBUG("%s %04x, a%d\n", ops3[op3], extra & 0xFFFF, ax);
                break;
            }

            DEBUG("? %04X\n", op);
            break;
        } else if((op & 0xFF70) == 0x8A60) {
            // TODO: norm
            u16 extra = FetchWord(pc+1);
            pc++;

            DEBUG("norm??\n");
            break;
        } else if((op & 0xF8E7) == 0x8060) {
            // maxd
            int d = op & (1 << 10);
            int f = op & (1 << 9);
            DEBUG("max%s %s, a%d\n", d ? "d" : "", f ? "gt" : "ge", ax);
            break;
        }

        DEBUG("? %04X\n", op);
        break;

    case 0xA:
    case 0xB:
        // ALM direct
        DEBUG("%s %02x, a%d\n", ops[(op >> 9) & 0xF], op & 0xFF, ax);
        break;

    case 0xC: {
        int op3 = HasOp3(op);

        if(op3 != -1) {
            // ALU #short immediate
            DEBUG("%s %02x, a%d\n", ops3[op3], op & 0xFF, ax);
            break;
        }
        DEBUG("? %04X\n", op);
        break;
    }


    case 0xD:
        if (op == 0xD390)
        {
            DEBUG("cntx r\n");
            break;
        }
        if (op == 0xD380)
        {
            DEBUG("cntx s\n");
            break;
        }
        if (op == 0xD7C0)
        {
            DEBUG("retid\n");
            break;
        }
        if (op == 0xD780)
        {
            DEBUG("retd\n");
            break;
        }
        if ((op & ~0xF100) == 0x480)
        {
            DEBUG("call a%d\n",ax);
            break;
        }
        if (op == 0xD3C0)
        {
            DEBUG("break\n");
            break;
        }
        if ((op & 0xF80) == 0xF80)
        {
            DEBUG("load stepj #%02x\n", op&0x7F);
            break;
        }
        if ((op & 0xF80) == 0xB80)
        {
            DEBUG("load stepi #%02x\n", op & 0x7F);
            break;
        }
        if ((op & 0xFEFC) == 0xD498)//1101010A100110--
        {
            u16 extra = FetchWord(pc + 1);
            pc++;
            DEBUG("mov (rb + #%04x), a%d\n", extra, ax);
            break;
        }
        if ((op & 0xFE80) == 0xDC80)//1101110a1ooooooo
        {
            DEBUG("mov (rb + #%02x), a%d\n", op&0x7F,ax);
            break;
        }
        if ((op & 0xFE80) == 0xD880)//1101100a1ooooooo
        {
            DEBUG("move a%dl, (rb + #%02x)\n", op & 0x7F, ax);
            break;
        }
        if ((op & 0xF39F) == 0xD290)//1101ab101AB10000
        {
            DEBUG("mov %s, %s\n", AB[(op >> 5) & 0x3], AB[(op >> 10) & 0x3]);
            break;
        }
        if ((op & 0xFF9F) == 0xD490)
        {
            DEBUG("mov repc, %s\n", AB[(op >> 5) & 0x3]);
            break;
        }
        if ((op & 0xFF9F) == 0xD491)
        {
            DEBUG("mov dvm, %s\n", AB[(op >> 5) & 0x3]);
            break;
        }
        if ((op & 0xFF9F) == 0xD492)
        {
            DEBUG("mov icr, %s\n", AB[(op >> 5) & 0x3]);
            break;
        }
        if ((op & 0xFF9F) == 0xD493)
        {
            DEBUG("mov x, %s\n", AB[(op >> 5) & 0x3]);
            break;
        }
        if ((op & 0xE80) == 0x080) {
            //msu (rJ), (rI) 1101000A1jjiiwqq
            DEBUG("msu r%d (modifier=%s),r%d (modifier=%s) a%d\n", op & 0x3, mm[(op >> 5) & 0x3], 3 + (op >> 2) & 0x1, mm[(op >> 3) & 0x3], ax);
            break;
        }
        if ((op & 0xFEFC) == 0xD4B8) //1101010a101110--
        {
            u16 extra = FetchWord(pc + 1);
            DEBUG("mov [%04x], a%d\n",extra,ax);
            pc++;
            break;
        }
        if ((op & 0xFEFC) == 0xD4BC) //1101010a101111--
        {
            u16 extra = FetchWord(pc + 1);
            DEBUG("mov a%dl, [%04x]\n", ax,extra);
            pc++;
            break;
        }

        if ((op & 0xF3FF) == 0xD2D8)
        {
            DEBUG("mov %s,x\n",AB[(op>>10) & 0x3]);
            break;
        }
        if ((op & 0xF3FF) == 0xD298)
        {
            DEBUG("mov %s,dvm\n", AB[(op >> 10) & 0x3]);
            break;
        }
        if (!(op & 0x80)) {
            //MUL (rJ), (rI) 1101AXXX0jjiiwqq
            DEBUG("%s r%d (modifier=%s),r%d (modifier=%s) a%d\n", mulXXX[(op >> 8) & 0x7], op & 0x3, mm[(op >> 5) & 0x3], 3 + (op >> 2) & 0x1, mm[(op >> 3) & 0x3], (op >> 11) & 0x1);
            break;
        }
        if ((op & 0xFE80) == 0xD080){
            //msu (rJ), (rI)
            DEBUG("msu r%d (modifier=%s),r%d (modifier=%s) a%d\n", op & 0x3, mm[(op >> 5) & 0x3], 3 + (op >> 2) & 0x1, mm[(op >> 3) & 0x3], ax);
            break;
        }
        if ((op & 0xF390) == 0xD280){
            //shfc
            DEBUG("shfc %s %s %s\n", AB[(op >> 10) & 0x3], AB[(op >> 5) & 0x3], cccc[op&0xF]);
            break;
        }
        if((op & 0xFED8) == 0xD4D8) {
            int op3 = HasOp3(op);

            if(op3 != -1) {
                u16 extra = FetchWord(pc+1);
                pc+=2;

                if(op & (1 << 5)) {  // ALU [##direct add.],ax
                    DEBUG("%s [%04x], a%d\n",
                          ops3[op3],
                          extra & 0xFFFF,
                          op & (0x1000) ? 1 : 0);
                    break;
                } else { // ALU (rb + ##offset),ax
                    DEBUG("%s (rb + %04x), a%d\n",
                          ops3[op3],
                          extra & 0xFFFF,
                          op & (0x1000) ? 1 : 0);
                    break;
                }
            }
        }
        DEBUG("? %04X\n", op);
        break;

    case 0xE:
        // ALB direct
        if(op & (1 << 8)) {
            u16 extra = FetchWord(pc+2);

            DEBUG("%s %02x, %04x\n", alb_ops[(op >> 9) & 0x7], op & 0xFF, extra & 0xFFFF);
            pc++;
            break;
        }
        else
        {
            DEBUG("%s a%d #%02x\n", mulXX[(op >> 9) & 0x3], (op >> 11) & 0x1,op&0xFF);
            break;
        }
        DEBUG("? %04X\n", op);
        break;
    case 0xF:
        DEBUG("tstb #%02x (bit=%d)\n", op&0xFF, (op >> 8) & 0xF);
        break;
    default:
        DEBUG("? %04X\n",op);
    }

    pc++;
}

void DSP_Run()
{
    pc = 0x0; //reset
    while (1)
    {
        DEBUG("op:%04x\n", FetchWord(pc));
        DSP_Step();
    }
}

void DSP_LoadFirm(u8* bin)
{
    dsp_header head;
    memcpy(&head, bin, sizeof(head));

    u32 magic = Read32(head.magic);
    u32 contsize = Read32(head.content_sz);
    u32 unk1 = Read32(head.unk1);
    u32 unk6 = Read32(head.unk6);
    u32 unk7 = Read32(head.unk7);

    DEBUG("head %08X %08X %08X %08X %08X %02X %02X %02X %02X\n",
          magic, contsize, unk1, unk6, unk7, head.unk2, head.unk3, head.num_sec, head.unk5);

    for (int i = 0; i < head.num_sec; i++) {
        u32 dataoffset = Read32(head.segment[i].data_offset);
        u32 destoffset = Read32(head.segment[i].dest_offset);
        u32 size = Read32(head.segment[i].size);
        u32 select = Read32(head.segment[i].select);

        DEBUG("segment %08X %08X %08X %08X\n", dataoffset, destoffset, size, select);
        memcpy(ram + (destoffset *2), bin + dataoffset, size);
    }

    DSP_Run();
}