/*
** Copyright (C) 1999 by Andreas Junghanns.
**
** Permission to use, copy, modify, and distribute this software and its
** documentation for any purpose and without fee is hereby granted, provided
** that the above copyright notice appear in all copies and that both that
** copyright notice and this permission notice appear in supporting
** documentation.  This software is provided "as is" without express or
** implied warranty.
*/

#include "board.h"
#include "fast_code_utils.h"
#include "experiments.h"
#include <immintrin.h>

void MarkAll(MAZE *maze) {

  SR(Debug(2,0,"MarkOut\n"));
  Set1BS(maze->out);
  MarkOut(maze,maze->manpos);

  SR(Debug(2,0,"MarkReach\n"));
  MarkReach(maze);

  SR(Debug(2,0,"MarkDead\n"));
  MarkDead(maze);

  SR(Debug(2,0,"SetAllStonesDone\n"));
  SetAllStonesDone(maze);

  SR(Debug(2,0,"MarkTun\n"));
  MarkTun(maze);

  SR(Debug(2,0,"GetNewDistances\n"));
  GetNewDistances(maze,NULL);

  SR(Debug(2,0,"SetDistDistances\n"));
  SetDistDistances(maze);

	SR(Debug(2,0,"SetDistDead\n"));
	SetDistDead(maze);

	SR(Debug(2,0,"MarkTG\n"));
	MarkTG(maze);
}

PHYSID   AvoidThisSquare=0;

void MarkTG(MAZE *maze) {
/* mark to goal, put the square to which all the goals are */
  int    i,j,k,count,dir1,dir2,dir3;
  TOGOAL to_goals[XSIZE*YSIZE];
  PHYSID goalloc,togoal;
  DIST   w;
  BitString zero, ones, stones;

  for (i=0; i<XSIZE*YSIZE; i++) {
    /* test for each goal, if it is in the same direction */
    to_goals[i].fw      = 0;
    to_goals[i].bw      = 0;
    if (!IsBitSetBS(maze->out,i)) {
      for (j=0; j<maze->number_goals; j++) {
        goalloc = maze->goals[j].loc;
        w = GetOptDist(maze,i,goalloc,NODIR);
        count = 0;
        togoal = 0;
        for (dir1=NORTH; dir1<=WEST; dir1++) {
          if (   IsBitSetBS(maze->S[dir1],i)
                    &&(w-1==GetOptDist(maze,
              i+DirToDiff[dir1],
              goalloc,
              OppDir[dir1]))) {
                count++;
                togoal = i+DirToDiff[dir1];;
          }
        }
        if (count!=1||(j>0&&to_goals[i].fw!=togoal)) {
            to_goals[i].fw = -ENDPATH;
            break;
        } else {
            to_goals[i].fw = togoal;
        }
      }
    } else {
      to_goals[i].fw = -ENDPATH;
    }
  }
  for (i=0; i<XSIZE*YSIZE; i++) {
    /* remove those that are against walls that would be
     * deadlocks anyways */
    if (IsBitSetBS(maze->out ,i)) continue;
    j = to_goals[i].fw;
    if (j != -ENDPATH) {
      if (j<0) j=-j;
      dir1 = j-i;
      k = to_goals[j].fw;
      if (k != -ENDPATH) {
        if (k<0) k=-k;
        dir2 = k - j;
        if (dir1 != dir2)
             to_goals[i].fw=-to_goals[i].fw;
      } else {
        to_goals[i].fw = -to_goals[i].fw;
      }
                        if (abs(dir1) == YSIZE) dir2=1;
                        else dir2=YSIZE;
                        if( (  (IsBitSetBS(maze->out,i+dir2))
                             ||(IsBitSetBS(maze->out,i-dir2)))
                           &&( (IsBitSetBS(maze->out,j+dir2))
                             ||(IsBitSetBS(maze->out,j-dir2)))) {
                                to_goals[i].fw = -to_goals[i].fw;
                        }
    }
    to_goals[i].bw = -ENDPATH;
  }
  for (i=0; i<XSIZE*YSIZE; i++) {
    /* chain th fw and bw of adjasent squares */
    if (IsBitSetBS(maze->out,i)) continue;
    j = to_goals[i].fw;
    if (j>0) {
      to_goals[j].bw = (PHYSID)i;
    }
    j = to_goals[i].bw;
    if (j>0) {
      to_goals[j].fw = (PHYSID)i;
    }
  }
  for (i=0; i<XSIZE*YSIZE; i++) {
    /* turn off those articulation squares like #2 low left that
     * are already penalized by backout heuristic */
    if (IsBitSetBS(maze->out,i)) continue;
    if (IsBitSetBS(maze->one_way,i)) {
      /* which direction is the chain */
      if (maze->Phys[i].free<4) continue;
      if ((j=to_goals[i].fw)>0) {
        dir1 = DiffToDir(j-i);
        /* all other dirs must be connected */
        for (dir2=NORTH;dir2<=WEST;dir2++) {
          dir3 = NextDir[dir2];
          if (  dir2!=dir1
              &&dir3!=dir1
              &&!ConnectedDir(maze,i,dir2,dir3)) {
            to_goals[i].fw=-ENDPATH;
            to_goals[j].bw=-ENDPATH;
          }
        }
      } else if ((j=to_goals[i].bw)>0) {
        dir1 = DiffToDir(j-i);
        /* all other dirs must be connected */
        for (dir2=NORTH;dir2<=WEST;dir2++) {
          dir3 = NextDir[dir2];
          if (  dir2!=dir1
              &&dir3!=dir1
              &&!ConnectedDir(maze,i,dir2,dir3)) {
            to_goals[i].bw=-ENDPATH;
            to_goals[j].fw=-ENDPATH;
          }
        }
      }
    }
  }

  Set0BS(zero);
  Set1BS(ones);
  for (i=0; i<XSIZE*YSIZE; i++) {
    /* finally, seed the conflict table */
    if (IsBitSetBS(maze->out,i)) continue;
    /* find the beginning of a chain */
    if (  (to_goals[i].fw>0)
        &&(to_goals[i].bw<=0)) {
      j=i;
      do {
        Set0BS(stones);
        SetBitBS(stones,j);
        SetBitBS(stones,to_goals[j].fw);
        AddConflict(maze->conflicts,stones,zero,2);
        j = to_goals[j].fw;
      } while (to_goals[j].fw>0);
    }
  }
}

void MarkOut(MAZE *maze, PHYSID pos) {
/* recursive function to unmark the inner fields from the out-flag */
	int dir;

	if (!IsBitSetBS(maze->out,pos)) return;
	UnsetBitBS(maze->out,pos);
	for (dir=NORTH; dir<=WEST; dir++) {
		if (IsBitSetBS(maze->M[dir],pos))
			MarkOut(maze, pos+DirToDiff[dir]);
	}
}

void CleanReach(MAZE *maze) {
  Set0BS(maze->reach);
}

void MarkReachImpl(MAZE *maze) {
  /* true implementation of MarkReach */
  /* recursive function to mark the fields that are reachable */

  static PHYSID stack[ENDPATH];
  PHYSID pos;
  int top;

  Set0BS(maze->reach); // This is a bitstring

  stack[0] = maze->manpos;
  top = 1;
  while( top ) {
    pos = stack[ --top ];

    // while counts
    //++mark_reach_while_counts;

    if( IsBitSetBS( maze->reach, pos) ) continue; // visited, bitstring 320 bits
    if( maze->PHYSstone[ pos ] >= 0 ) continue;
    if( AvoidThisSquare == pos ) continue;

    SetBitBS( maze->reach,pos );

    if( IsBitSetBS( maze->M[ 0 ], pos ) )
      stack[ top++ ] = pos + 1;
    if( IsBitSetBS( maze->M[ 1 ], pos ) )
      stack[ top++ ] = pos + YSIZE;
    if( IsBitSetBS( maze->M[ 2 ], pos ) )
      stack[ top++ ] = pos - 1;
    if( IsBitSetBS( maze->M[ 3 ], pos ) )
      stack[ top++ ] = pos - YSIZE;
  }
  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

void MarkReachImp4(MAZE *maze) {
  /* true implementation of MarkReach */
  /* recursive function to mark the fields that are reachable */

  char buf[64] = {0};
  memcpy(buf, maze->wall, 40);

  __m256i temp, temp1;
  __m256i map0 = _mm256_loadu_si256((void *)buf);
  __m256i map1 = _mm256_loadu_si256((void *)(buf + 32));

  // Make the stone bitvectors fold into the map
  memset(buf, 0, 40);
  for (int i = 0; i < 320; i++) {
    if (maze->PHYSstone[ i ] >= 0) {
      SetBitBS((BASETYPE*)buf, i);
    }
  }
  SetBitBS((BASETYPE*)buf, AvoidThisSquare);
  UnsetBitBS((BASETYPE*)buf, maze->manpos);

  temp = _mm256_loadu_si256((void *)buf);
  temp1 = _mm256_loadu_si256((void *)(buf + 32));

  map0 = _mm256_or_si256(map0, temp);
  map1 = _mm256_or_si256(map1, temp1);

  memcpy(buf, maze->out, 40);

  temp = _mm256_loadu_si256((void *)buf);
  temp1 = _mm256_loadu_si256((void *)(buf + 32));

  map0 = _mm256_or_si256(map0, temp);
  map1 = _mm256_or_si256(map1, temp1);

  memset(buf, 0, 64); // This is a bitstring

  __m256i reach0B = _mm256_loadu_si256((void *)buf);
  __m256i reach1B = _mm256_loadu_si256((void *)(buf + 32));

  SetBitBS((BASETYPE*)buf, maze->manpos);

  __m256i reach0A = _mm256_loadu_si256((void *)buf);
  __m256i reach1A = _mm256_loadu_si256((void *)(buf + 32));

  int carry;
  int done0, done1;

  while (1) {

    reach0B = _mm256_blend_epi16(reach0A, temp, 0);
    reach1B = _mm256_blend_epi16(reach1A, temp, 0);
    temp = _mm256_slli_epi16(reach0A, 1);
    reach0B = _mm256_or_si256(reach0B, temp);
    temp = _mm256_srli_epi16(reach0A, 1);
    reach0B = _mm256_or_si256(reach0B, temp);

    temp = _mm256_slli_epi16(reach1A, 1);
    reach1B = _mm256_or_si256(reach1B, temp);
    temp = _mm256_srli_epi16(reach1A, 1);
    reach1B = _mm256_or_si256(reach1B, temp);

    // Do the left shift by 16
    temp = _mm256_slli_si256(reach1A, 2);
    carry = _mm256_extract_epi16(reach0A, 15);
    temp = _mm256_insert_epi16(temp, carry, 0);
    reach1B = _mm256_or_si256(reach1B, temp);

    temp = _mm256_slli_si256(reach0A, 2);
    carry = _mm256_extract_epi16(reach0A, 7);
    temp = _mm256_insert_epi16(temp, carry, 8);
    reach0B = _mm256_or_si256(reach0B, temp);

    // Do the right shift by 16
    temp = _mm256_srli_si256(reach0A, 2);
    carry = _mm256_extract_epi16(reach0A, 8);
    temp = _mm256_insert_epi16(temp, carry, 7);
    carry = _mm256_extract_epi16(reach1A, 0);
    temp = _mm256_insert_epi16(temp, carry, 15);
    reach0B = _mm256_or_si256(reach0B, temp);

    temp = _mm256_srli_si256(reach1A, 2);
    reach1B = _mm256_or_si256(reach1B, temp);

    reach0B = _mm256_andnot_si256(map0, reach0B);
    reach1B = _mm256_andnot_si256(map1, reach1B);

    temp = _mm256_xor_si256(reach0A, reach0B);
    done0 = _mm256_testz_si256(temp, temp);
    temp = _mm256_xor_si256(reach1A, reach1B);
    done1 = _mm256_testz_si256(temp, temp);

    if (done0 && done1) break;

    reach0A = _mm256_blend_epi16(reach0B, temp, 0);
    reach1A = _mm256_blend_epi16(reach1B, temp, 0);

    temp = _mm256_slli_epi16(reach0B, 1);
    reach0A = _mm256_or_si256(reach0A, temp);
    temp = _mm256_srli_epi16(reach0B, 1);
    reach0A = _mm256_or_si256(reach0A, temp);

    temp = _mm256_slli_epi16(reach1B, 1);
    reach1A = _mm256_or_si256(reach1A, temp);
    temp = _mm256_srli_epi16(reach1B, 1);
    reach1A = _mm256_or_si256(reach1A, temp);

    // Do the left shift by 16
    temp = _mm256_slli_si256(reach1B, 2);
    carry = _mm256_extract_epi16(reach0B, 15);
    temp = _mm256_insert_epi16(temp, carry, 0);
    reach1A = _mm256_or_si256(reach1A, temp);

    temp = _mm256_slli_si256(reach0B, 2);
    carry = _mm256_extract_epi16(reach0B, 7);
    temp = _mm256_insert_epi16(temp, carry, 8);
    reach0A = _mm256_or_si256(reach0A, temp);

    // Do the right shift by 16

    temp = _mm256_srli_si256(reach0B, 2);
    carry = _mm256_extract_epi16(reach0B, 8);
    temp = _mm256_insert_epi16(temp, carry, 7);
    carry = _mm256_extract_epi16(reach1B, 0);
    temp = _mm256_insert_epi16(temp, carry, 15);
    reach0A = _mm256_or_si256(reach0A, temp);

    temp = _mm256_srli_si256(reach1B, 2);
    reach1A = _mm256_or_si256(reach1A, temp);

    reach0A = _mm256_andnot_si256(map0, reach0A);
    reach1A = _mm256_andnot_si256(map1, reach1A);

    temp = _mm256_xor_si256(reach0A, reach0B);
    done0 = _mm256_testz_si256(temp, temp);
    temp = _mm256_xor_si256(reach1A, reach1B);
    done1 = _mm256_testz_si256(temp, temp);

    if (done0 && done1) break;

  }

  _mm256_storeu_si256((void *)buf, reach0A);
  _mm256_storeu_si256((void *)(buf + 32), reach1A);

  memcpy(maze->reach, buf, 40);

  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

void MarkReachImp5(MAZE *maze) {
  /* true implementation of MarkReach */
  /* recursive function to mark the fields that are reachable */

  char buf[64] = {0};
  memcpy(buf, maze->wall, 40);

  __m256i temp, temp1;
  __m256i map0 = _mm256_loadu_si256((void *)buf);
  __m256i map1 = _mm256_loadu_si256((void *)(buf + 32));

  // Make the stone bitvectors fold into the map

  memset(buf, 0, 40);
  for (int i = 0; i < maze->number_stones; i++) {
    if (maze->PHYSstone[maze->stones[i].loc] >= 0) {
      SetBitBS((BASETYPE*)buf, maze->stones[i].loc);
    }
  }
  //mark_reach_stone_counts += maze->number_stones;

  SetBitBS((BASETYPE*)buf, AvoidThisSquare);
  UnsetBitBS((BASETYPE*)buf, maze->manpos);

  temp = _mm256_loadu_si256((void *)buf);
  temp1 = _mm256_loadu_si256((void *)(buf + 32));

  map0 = _mm256_or_si256(map0, temp);
  map1 = _mm256_or_si256(map1, temp1);

  memcpy(buf, maze->out, 40);

  temp = _mm256_loadu_si256((void *)buf);
  temp1 = _mm256_loadu_si256((void *)(buf + 32));

  map0 = _mm256_or_si256(map0, temp);
  map1 = _mm256_or_si256(map1, temp1);

  memset(buf, 0, 64); // This is a bitstring

  __m256i reach0B = _mm256_loadu_si256((void *)buf);
  __m256i reach1B = _mm256_loadu_si256((void *)(buf + 32));

  SetBitBS((BASETYPE*)buf, maze->manpos);

  __m256i reach0A = _mm256_loadu_si256((void *)buf);
  __m256i reach1A = _mm256_loadu_si256((void *)(buf + 32));

  int carry;
  int done0, done1;

  while (1) {
    //mark_reach_while_counts++;
    reach0B = _mm256_blend_epi16(reach0A, temp, 0);
    reach1B = _mm256_blend_epi16(reach1A, temp, 0);
    temp = _mm256_slli_epi16(reach0A, 1);
    reach0B = _mm256_or_si256(reach0B, temp);
    temp = _mm256_srli_epi16(reach0A, 1);
    reach0B = _mm256_or_si256(reach0B, temp);

    temp = _mm256_slli_epi16(reach1A, 1);
    reach1B = _mm256_or_si256(reach1B, temp);
    temp = _mm256_srli_epi16(reach1A, 1);
    reach1B = _mm256_or_si256(reach1B, temp);

    // Do the left shift by 16
    temp = _mm256_slli_si256(reach1A, 2);
    carry = _mm256_extract_epi16(reach0A, 15);
    temp = _mm256_insert_epi16(temp, carry, 0);
    reach1B = _mm256_or_si256(reach1B, temp);

    temp = _mm256_slli_si256(reach0A, 2);
    carry = _mm256_extract_epi16(reach0A, 7);
    temp = _mm256_insert_epi16(temp, carry, 8);
    reach0B = _mm256_or_si256(reach0B, temp);

    // Do the right shift by 16
    temp = _mm256_srli_si256(reach0A, 2);
    carry = _mm256_extract_epi16(reach0A, 8);
    temp = _mm256_insert_epi16(temp, carry, 7);
    carry = _mm256_extract_epi16(reach1A, 0);
    temp = _mm256_insert_epi16(temp, carry, 15);
    reach0B = _mm256_or_si256(reach0B, temp);

    temp = _mm256_srli_si256(reach1A, 2);
    reach1B = _mm256_or_si256(reach1B, temp);

    reach0B = _mm256_andnot_si256(map0, reach0B);
    reach1B = _mm256_andnot_si256(map1, reach1B);

    temp = _mm256_xor_si256(reach0A, reach0B);
    done0 = _mm256_testz_si256(temp, temp);
    temp = _mm256_xor_si256(reach1A, reach1B);
    done1 = _mm256_testz_si256(temp, temp);

    if (done0 && done1) break;
    //mark_reach_while_counts++;

    reach0A = _mm256_blend_epi16(reach0B, temp, 0);
    reach1A = _mm256_blend_epi16(reach1B, temp, 0);

    temp = _mm256_slli_epi16(reach0B, 1);
    reach0A = _mm256_or_si256(reach0A, temp);
    temp = _mm256_srli_epi16(reach0B, 1);
    reach0A = _mm256_or_si256(reach0A, temp);

    temp = _mm256_slli_epi16(reach1B, 1);
    reach1A = _mm256_or_si256(reach1A, temp);
    temp = _mm256_srli_epi16(reach1B, 1);
    reach1A = _mm256_or_si256(reach1A, temp);

    // Do the left shift by 16
    temp = _mm256_slli_si256(reach1B, 2);
    carry = _mm256_extract_epi16(reach0B, 15);
    temp = _mm256_insert_epi16(temp, carry, 0);
    reach1A = _mm256_or_si256(reach1A, temp);

    temp = _mm256_slli_si256(reach0B, 2);
    carry = _mm256_extract_epi16(reach0B, 7);
    temp = _mm256_insert_epi16(temp, carry, 8);
    reach0A = _mm256_or_si256(reach0A, temp);

    // Do the right shift by 16

    temp = _mm256_srli_si256(reach0B, 2);
    carry = _mm256_extract_epi16(reach0B, 8);
    temp = _mm256_insert_epi16(temp, carry, 7);
    carry = _mm256_extract_epi16(reach1B, 0);
    temp = _mm256_insert_epi16(temp, carry, 15);
    reach0A = _mm256_or_si256(reach0A, temp);

    temp = _mm256_srli_si256(reach1B, 2);
    reach1A = _mm256_or_si256(reach1A, temp);

    reach0A = _mm256_andnot_si256(map0, reach0A);
    reach1A = _mm256_andnot_si256(map1, reach1A);

    temp = _mm256_xor_si256(reach0A, reach0B);
    done0 = _mm256_testz_si256(temp, temp);
    temp = _mm256_xor_si256(reach1A, reach1B);
    done1 = _mm256_testz_si256(temp, temp);

    if (done0 && done1) break;

  }

  _mm256_storeu_si256((void *)buf, reach0A);
  _mm256_storeu_si256((void *)(buf + 32), reach1A);

  memcpy(maze->reach, buf, 40);

  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

void MarkReachImp6(MAZE *maze) {
  /* true implementation of MarkReach */
  /* recursive function to mark the fields that are reachable */


  __m256i temp, temp1, shufmask, zeros, map0, map1;
  __m256i reach0A, reach1A, reach0B, reach1B;
  char buf[64] = {0};
  zeros = _mm256_setzero_si256();  // vpxor  1/3

  SetBitBS((BASETYPE*)buf, maze->manpos);
  reach0A = _mm256_loadu_si256((void *)buf);        // vmovdqu 3/2
  reach0B = _mm256_loadu_si256((void *)buf);        // vmovdqu 3/2
  reach1A = _mm256_loadu_si256((void *)(buf + 32)); // vmovdqu 3/2
  reach1B = _mm256_loadu_si256((void *)(buf + 32)); // vmovdqu 3/2
  UnsetBitBS((BASETYPE*)buf, maze->manpos);

  // Make the stone bitvectors fold into the map

  static const unsigned char __shufmask[32] = {
    0x0E, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01,
    0x0E, 0x0F, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00, 0x01
  };

  shufmask = _mm256_loadu_si256((void *)__shufmask); // vmovdqu 3/2

  memcpy(buf, maze->out, 40);
  for (int i = 0; i < maze->number_stones; i++) {
    if (maze->PHYSstone[maze->stones[i].loc] >= 0) {
      SetBitBS((BASETYPE*)buf, maze->stones[i].loc);
    }
  }

  SetBitBS((BASETYPE*)buf, AvoidThisSquare);
  UnsetBitBS((BASETYPE*)buf, maze->manpos);

  map0 = _mm256_loadu_si256((void *)buf);        // vmovdqu 3/2
  map1 = _mm256_loadu_si256((void *)(buf + 32)); // vmovdqu 3/2

  int done0 = 0, done1 = 0;

  while (!(done0 && done1)) {
    
    temp = _mm256_slli_epi16(reach0A, 1);      // vpsllw 1/1
    temp1 = _mm256_slli_epi16(reach1A, 1);     // vpsllw 1/1
    reach0B = _mm256_or_si256(reach0B, temp);  // vpor 1/3
    reach1B = _mm256_or_si256(reach1B, temp1); // vpor 1/3
    temp = _mm256_srli_epi16(reach0A, 1);      // vpsllw 1/1
    temp1 = _mm256_srli_epi16(reach1A, 1);     // vpsllw 1/1
    reach0B = _mm256_or_si256(reach0B, temp);  // vpor 1/3
    reach1B = _mm256_or_si256(reach1B, temp1); // vpor 1/3

    // settle the carries from the l/r shift by 16
    temp = _mm256_shuffle_epi8(reach0A, shufmask);  // vpshufb 1/1
    temp1 = _mm256_shuffle_epi8(reach1A, shufmask); // vpshufb 1/1
    temp = _mm256_permute2x128_si256(temp, temp, 0x01); // vperm2i128 3/1
    temp1 = _mm256_permute2x128_si256(temp1, temp1, 0x01); // vperm2i128 3/1
    zeros = _mm256_blend_epi32(zeros, temp, 0b1);   // vpblendd 1/3
    temp = _mm256_blend_epi32(temp, temp1, 0b10000001); // vpblendd 1/3
    reach1B = _mm256_or_si256(reach1B, zeros);      // vpor 1/3
    reach0B = _mm256_or_si256(reach0B, temp);       // vpor 1/3

    // shift left by 16
    temp = _mm256_slli_si256(reach0A, 2);           // vpslldq 1/1
    temp1 = _mm256_slli_si256(reach1A, 2);          // vpslldq 1/1
    reach0B = _mm256_or_si256(reach0B, temp);       // vpor 1/3
    reach1B = _mm256_or_si256(reach1B, temp1);      // vpor 1/3
    // shift right by 16
    temp = _mm256_srli_si256(reach0A, 2);           // vpsrldq 1/1
    temp1 = _mm256_srli_si256(reach1A, 2);          // vpsrldq 1/1
    reach0B = _mm256_or_si256(reach0B, temp);       // vpor 1/3
    reach1B = _mm256_or_si256(reach1B, temp1);      // vpor 1/3

    reach0B = _mm256_andnot_si256(map0, reach0B);   // vpandn 1/3
    reach1B = _mm256_andnot_si256(map1, reach1B);   // vpandn 1/3

    temp = _mm256_xor_si256(reach0A, reach0B);      // vpxor 1/3
    temp1 = _mm256_xor_si256(reach1A, reach1B);     // vpxor 1/3
    done0 = _mm256_testz_si256(temp, temp);         // vptest 2/1
    done1 = _mm256_testz_si256(temp1, temp1);       // vptest 2/1

    // swap with the break to hide latency if not breaking
    reach0A = reach0B;
    reach1A = reach1B;  // vmovdqu 1/3
    if (done0 && done1) break;

    temp = _mm256_slli_epi16(reach0B, 1);      // vpsllw 1/1
    temp1 = _mm256_slli_epi16(reach1B, 1);     // vpsllw 1/1
    reach0A = _mm256_or_si256(reach0A, temp);  // vpor 1/3
    reach1A = _mm256_or_si256(reach1A, temp1); // vpor 1/3
    temp = _mm256_srli_epi16(reach0B, 1);      // vpsllw 1/1
    temp1 = _mm256_srli_epi16(reach1B, 1);     // vpsllw 1/1
    reach0A = _mm256_or_si256(reach0A, temp);  // vpor 1/3
    reach1A = _mm256_or_si256(reach1A, temp1); // vpor 1/3

    // settle the carries from the l/r shift by 16
    temp = _mm256_shuffle_epi8(reach0B, shufmask);  // vpshufb 1/1
    temp1 = _mm256_shuffle_epi8(reach1B, shufmask); // vpshufb 1/1
    temp = _mm256_permute2x128_si256(temp, temp, 0x01); // vperm2i128 3/1
    temp1 = _mm256_permute2x128_si256(temp1, temp1, 0x01); // vperm2i128 3/1
    zeros = _mm256_blend_epi32(zeros, temp, 0b1);   // vpblendd 1/3
    temp = _mm256_blend_epi32(temp, temp1, 0b10000001); // vpblendd 1/3
    reach1A = _mm256_or_si256(reach1A, zeros);      // vpor 1/3
    reach0A = _mm256_or_si256(reach0A, temp);       // vpor 1/3

    // shift left by 16
    temp = _mm256_slli_si256(reach0B, 2);           // vpslldq 1/1
    temp1 = _mm256_slli_si256(reach1B, 2);          // vpslldq 1/1
    reach0A = _mm256_or_si256(reach0A, temp);       // vpor 1/3
    reach1A = _mm256_or_si256(reach1A, temp1);      // vpor 1/3
    // shift right by 16
    temp = _mm256_srli_si256(reach0B, 2);           // vpsrldq 1/1
    temp1 = _mm256_srli_si256(reach1B, 2);          // vpsrldq 1/1
    reach0A = _mm256_or_si256(reach0A, temp);       // vpor 1/3
    reach1A = _mm256_or_si256(reach1A, temp1);      // vpor 1/3

    reach0A = _mm256_andnot_si256(map0, reach0A);   // vpandn 1/3
    reach1A = _mm256_andnot_si256(map1, reach1A);   // vpandn 1/3

    temp = _mm256_xor_si256(reach0A, reach0B);      // vpxor 1/3
    temp1 = _mm256_xor_si256(reach1A, reach1B);     // vpxor 1/3
    done0 = _mm256_testz_si256(temp, temp);         // vptest 2/1
    done1 = _mm256_testz_si256(temp1, temp1);       // vptest 2/1

    reach0B = reach0A;  // swap with the break to hide latency if not breaking
    reach1B = reach1A;

  }

  _mm256_storeu_si256((void *)buf, reach0A);
  _mm256_storeu_si256((void *)(buf + 32), reach1A);

  memcpy(maze->reach, buf, 40);

  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

void MarkReachImp2(MAZE *maze) {
  /* DFS function to mark the fields that are reachable */

  static PHYSID stack[ENDPATH];
  PHYSID pos;
  int top;

  Set0BS(maze->reach); // This is a bitstring

  stack[0] = maze->manpos;
  top = 1;
  while( top ) {
    pos = stack[ --top ];
    if( IsBitSetBS( maze->reach, pos) ) continue; // visited, bitstring 320 bits
    if( maze->PHYSstone[ pos ] >= 0 ) continue;
    if( AvoidThisSquare == pos ) continue;
    SetBitBS( maze->reach,pos );

    if( IsBitSetBS( maze->M[ 1 ], pos ) )
    stack[ top++ ] = pos + YSIZE;
    if( IsBitSetBS( maze->M[ 3 ], pos ) )
    stack[ top++ ] = pos - YSIZE;

    PHYSID pos_temp = pos;
    while( IsBitSetBS( maze->M[ 0 ], pos_temp ) ) {
      pos_temp++;
      if( maze->PHYSstone[ pos_temp ] >= 0 ) break;
      if( AvoidThisSquare == pos_temp ) break;
      SetBitBS( maze->reach, pos_temp );
      if( IsBitSetBS( maze->M[ 1 ], pos_temp ) )
        stack[ top++ ] = pos_temp + YSIZE;
      if( IsBitSetBS( maze->M[ 3 ], pos_temp ) )
        stack[ top++ ] = pos_temp - YSIZE;
    }
    pos_temp = pos;
    while( IsBitSetBS( maze->M[ 2 ], pos_temp ) ) {
      pos_temp--;
      if( maze->PHYSstone[ pos_temp ] >= 0 ) break;
      if( AvoidThisSquare == pos_temp ) break;
      SetBitBS( maze->reach, pos_temp );
      if( IsBitSetBS( maze->M[ 1 ], pos_temp ) )
        stack[ top++ ] = pos_temp + YSIZE;
      if( IsBitSetBS( maze->M[ 3 ], pos_temp ) )
        stack[ top++ ] = pos_temp - YSIZE;
    }
  }
  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

void MarkReachImp3(MAZE *maze) {
  /* true implementation of MarkReach */
  /* recursive function to mark the fields that are reachable */

  static PHYSID queue[ENDPATH];
  PHYSID pos;
  int bot, top;

  Set0BS(maze->reach); // This is a bitstring

  queue[0] = maze->manpos;
  bot = 0;
  top = 1;
  while( bot < top ) {
    // while counts
    ++mark_reach_while_counts;
    pos = queue[ bot++ ];
    if( IsBitSetBS( maze->reach, pos) ) continue; // visited, bitstring 320 bits
    if( maze->PHYSstone[ pos ] >= 0 ) continue;
    if( AvoidThisSquare == pos ) continue;

    SetBitBS( maze->reach,pos );

    if( IsBitSetBS( maze->M[ 0 ], pos ) )
      queue[ top++ ] = pos + 1;
    if( IsBitSetBS( maze->M[ 1 ], pos ) )
      queue[ top++ ] = pos + YSIZE;
    if( IsBitSetBS( maze->M[ 2 ], pos ) )
      queue[ top++ ] = pos - 1;
    if( IsBitSetBS( maze->M[ 3 ], pos ) )
      queue[ top++ ] = pos - YSIZE;
  }
  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

//#define CHECKER
void MarkReach(MAZE *maze) {
  /* a wrapper for counting number of cycles in MarkReach */
  ++mark_reach_counts;
  unsigned long long cnt = rdtsc();

  MarkReachImp6(maze);
  mark_reach_cycles += rdtsc() - cnt;
#ifdef CHECKER

  char checkbuf[40];
  char refbuf[40];
  memcpy(checkbuf, maze->reach, 40);
  MarkReachImpl(maze);
  memcpy(refbuf, maze->reach, 40);

  int i;
  for (i = 0; i < 40; i++) {
    if (checkbuf[i] != refbuf[i]) {
      printf("MISMATCH\n");
      printf("Ref:\n");
      for (int i = 0; i < 10; i++) {
        printf("%08x ", ((int *)refbuf)[i]);
      }
      printf("\nObs:\n");
      for (int i = 0; i < 10; i++) {
        printf("%08x ", ((int *)checkbuf)[i]);
      }
      printf("\n");
      printf("%d", maze->PHYSstone[ maze->manpos ]);
      exit(-1);
    }
  }

#endif

}

void MarkReachNoUnreach(MAZE *maze) {
  /* recursive function to mark the fields that are reachable */

  static PHYSID stack[ENDPATH];
  PHYSID pos;
  int top;

  Set0BS(maze->reach);

  stack[0] = maze->manpos;
  top = 1;
  while( top ) {
    pos = stack[ --top ];
    if( IsBitSetBS( maze->reach, pos) ) continue;
    if( maze->PHYSstone[ pos ] >= 0 ) continue;

    SetBitBS( maze->reach,pos );

    if( IsBitSetBS( maze->M[ 0 ], pos ) )
      stack[ top++ ] = pos + 1;
    if( IsBitSetBS( maze->M[ 1 ], pos ) )
      stack[ top++ ] = pos + YSIZE;
    if( IsBitSetBS( maze->M[ 2 ], pos ) )
      stack[ top++ ] = pos - 1;
    if( IsBitSetBS( maze->M[ 3 ], pos ) )
      stack[ top++ ] = pos - YSIZE;
  }
  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

void MarkReachQuick( MAZE *maze, PHYSID from ) {
  /* recursive function to mark the fields that are reachable */

  static PHYSID stack[ENDPATH];
  PHYSID pos;
  int top;

  UnsetBitBS( maze->reach, from );

  stack[0] = from;
  top = 1;
  while( top ) {
    pos = stack[ --top ];
    if( IsBitSetBS( maze->reach, pos) ) continue;
    if( maze->PHYSstone[ pos ] >= 0 ) continue;
    if( AvoidThisSquare == pos ) continue;

    SetBitBS( maze->reach,pos );

    if( IsBitSetBS( maze->M[ 0 ], pos ) )
      stack[ top++ ] = pos + 1;
    if( IsBitSetBS( maze->M[ 1 ], pos ) )
      stack[ top++ ] = pos + YSIZE;
    if( IsBitSetBS( maze->M[ 2 ], pos ) )
      stack[ top++ ] = pos - 1;
    if( IsBitSetBS( maze->M[ 3 ], pos ) )
      stack[ top++ ] = pos - YSIZE;
  }
  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

int UnReach( MAZE *maze, PHYSID start, BitString treach ) {
  static PHYSID queue[ ENDPATH ];
  PHYSID pos;
  int top, bottom;

  queue[ 0 ] = start;
  top = 1;
  bottom = -1;
  while( ++bottom < top ) {
    pos = queue[ bottom ];

    if( pos == maze->manpos )
      return 1;

    if( !IsBitSetBS( maze->reach, pos ) ||
  IsBitSetBS( treach, pos ) ||
  maze->PHYSstone[ pos ] >= 0 ||
  AvoidThisSquare == pos )
      continue;

    SetBitBS( treach, pos );

    if( IsBitSetBS( maze->M[ 0 ], pos ) )
      queue[ top++ ] = pos + 1;
    if( IsBitSetBS( maze->M[ 1 ], pos ) )
      queue[ top++ ] = pos + YSIZE;
    if( IsBitSetBS( maze->M[ 2 ], pos ) )
      queue[ top++ ] = pos - 1;
    if( IsBitSetBS( maze->M[ 3 ], pos ) )
      queue[ top++ ] = pos - YSIZE;
  }

  BitAndNotEqBS( maze->reach, treach );
  return 0;
}

void UpdateReach( MAZE *maze, PHYSID stonepos )
{
  static BitString treach;
  char a, b, c;

  MarkReachQuick( maze, maze->manpos );

  /* only need to clear treach if UnReach discards calculations */
  a = b = c = 1;
  UnsetBitBS( maze->reach, stonepos );
  Set0BS( treach );
  if( stonepos + 1 != maze->manpos &&
      UnReach( maze, stonepos + 1, treach ) ) {
    if( IsBitSetBS( treach, stonepos + YSIZE ) &&
  IsBitSetBS( maze->reach, stonepos + YSIZE ) )
      a = 0;
    if( IsBitSetBS( treach, stonepos - 1 ) &&
  IsBitSetBS( maze->reach, stonepos - 1 ) )
      b = 0;
    if( IsBitSetBS( treach, stonepos - YSIZE ) &&
  IsBitSetBS( maze->reach, stonepos - YSIZE ) )
      c = 0;
    Set0BS( treach );
  }
  if( a &&
      stonepos + YSIZE != maze->manpos &&
      UnReach( maze, stonepos + YSIZE, treach ) ) {
    if( IsBitSetBS( treach, stonepos - 1 ) &&
  IsBitSetBS( maze->reach, stonepos - 1 ) )
      b = 0;
    if( IsBitSetBS( treach, stonepos - YSIZE ) &&
  IsBitSetBS( maze->reach, stonepos - YSIZE ) )
      c = 0;
    Set0BS( treach );
  }
  if( b &&
      stonepos - 1 != maze->manpos &&
      UnReach( maze, stonepos - 1, treach ) ) {
    if( IsBitSetBS( treach, stonepos - YSIZE ) &&
  IsBitSetBS( maze->reach, stonepos - YSIZE ) )
      c = 0;
    Set0BS( treach );
  }
  if( c
      && stonepos - YSIZE != maze->manpos )
    UnReach( maze, stonepos - YSIZE, treach );

  BitNotAndNotAndNotBS(maze->no_reach,maze->reach,maze->out,maze->stone);
}

void MarkDead(MAZE *maze) {
/* function to mark the fields that create a deadlock if a stone is on  */

  PHYSID pos,p;
  int dead,dir;

  for (pos=0; pos<XSIZE*YSIZE; pos++) {
    if (IsBitSetBS(maze->out,pos)) continue;
    if (maze->Phys[pos].goal>=0) continue;
    if (!( ( IsBitSetBS( maze->M[NORTH], pos ) &&
       IsBitSetBS( maze->M[SOUTH], pos ) ) ||
           ( IsBitSetBS( maze->M[WEST], pos ) &&
       IsBitSetBS( maze->M[EAST], pos ) ) ) ) {
      SetBitBS(maze->dead,pos);
      for (dir=NORTH; dir<=WEST; dir++) {
        if (IsBitSetBS(maze->M[dir],pos))
          UnsetBitBS(maze->S[OppDir[dir]],
               pos+DirToDiff[dir]);
      }
    }
    /* check x direction only in east, since we start */
    /* lower left corner and move right */
    /* we assume dead and if not found otherwise mark */
    if (!IsBitSetBS(maze->M[WEST],pos)) dead = 1;
    else dead = 0;
    for (p=pos; dead==1 && p<YSIZE*XSIZE; p+=YSIZE) {
      /* if we hit goal state, not dead */
      if (maze->Phys[p].goal>=0) {
        dead = 0;
        break;
      }
      /* check if both N and S are possible and break */
      if (IsBitSetBS(maze->M[NORTH],p)
          &&IsBitSetBS(maze->M[SOUTH],p)) {
        dead = 0;
        break;
      }
      /* check if end */
      if (!IsBitSetBS(maze->M[EAST],p)) break;
    }
    /* if dead is still 1, we rerun pos and set dead flag */
    if (dead==1) for (p=pos; p<YSIZE*XSIZE; p+=YSIZE) {
      SetBitBS(maze->dead,p);
      /* set neighbours S[NEWS] flags */
      for (dir=NORTH; dir<=WEST; dir++) {
        if (IsBitSetBS(maze->M[dir],p))
          UnsetBitBS(maze->S[OppDir[dir]],
               p+DirToDiff[dir]);
      }
      if (!IsBitSetBS(maze->M[EAST],p)) break;
    }
    /* now same for E/W, asume south was done */
    if (!IsBitSetBS(maze->M[SOUTH],pos)) dead = 1;
    else dead = 0;
    for (p=pos; dead==1 && (p%YSIZE)!=0; p++) {
      /* if we hit goal state, not dead */
      if (maze->Phys[p].goal>=0) {
        dead = 0;
        break;
      }
      /* check if both W and E are possible and break */
      if (  IsBitSetBS(maze->M[WEST],p)
          &&IsBitSetBS(maze->M[EAST],p)) {
        dead = 0;
        break;
      }
      /* check if end */
      if (!IsBitSetBS(maze->M[NORTH],p)) break;
    }
    /* if dead is still 1, we rerun pos and set dead flag */
    if (dead==1) for (p=pos; (p%YSIZE)!=0; p++) {
      SetBitBS(maze->dead,p);
      /* set neighbours S[NEWS] flags */
      for (dir=NORTH; dir<=WEST; dir++) {
        if (IsBitSetBS(maze->M[dir],p))
          UnsetBitBS(maze->S[OppDir[dir]],
               p+DirToDiff[dir]);
      }
      if (!IsBitSetBS(maze->M[NORTH],p)) break;
    }
  }
}

void MarkOneConnected(MAZE *maze)
/* mark Oneway and Connected, assume obstacles contain walls and out */
{
  int i,dir,dir2;
  BitString obstacles;

  /* Initialize both */
  Set0BS(maze->one_way);
  maze->connected      = (CONN*)My_malloc(sizeof(CONN));
  memset(maze->connected,0,sizeof(CONN));

  BitOrBS(obstacles,maze->stones_done,maze->out);
  for (i=0; i<XSIZE*YSIZE; i++) {
    if (IsBitSetBS(obstacles,i)) continue;
    for (dir=NORTH; dir<=WEST; dir++) {

      /* if no obstacle there and not touched already */
      if (!IsBitSetBS(obstacles,i+DirToDiff[dir])) {
          (*maze->connected)[dir][i]|=1<<dir;

          MarkReachDist(maze,i+DirToDiff[dir],i,obstacles);
          for (dir2=dir+1; dir2<=WEST; dir2++) {
        if (IsBitSetBS(maze->reach,i+DirToDiff[dir2])) {
            /* both directions are connected */
            (*maze->connected)[dir][i]|=1<<dir2;
            (*maze->connected)[dir2][i]|=1<<dir;
        } else if (!IsBitSetBS(obstacles,
                   i+DirToDiff[dir2])) {
            /* these are not connected-> oneway */
            SetBitBS(maze->one_way,i);
        }
          }
      }
    }
  }
}

void MarkTun(MAZE *maze) {

  int i,dim,j,dir,tunnel;
  short min_dim=0;

  for (i=0; i<XSIZE*YSIZE; i++) {
    if (IsBitSetBS(maze->out,i)) continue;
    maze->Phys[i].tunnel = 0;
    maze->Phys[i].min_dim = 0;
    maze->Phys[i].free = 0;
    maze->Phys[i].s_free = 0;

    for (dir=NORTH; dir<=WEST; dir++) {
      if (IsBitSetBS(maze->S[dir],i)) maze->Phys[i].s_free++;
      if (IsBitSetBS(maze->M[dir],i)) maze->Phys[i].free++;
    }
    /* collect both dimensions man tunnels */
    tunnel = YSIZE*XSIZE;

    dim=1;
    j=i;
    j+=YSIZE;
    while (!IsBitSetBS(maze->out,j)) { dim++; j+=YSIZE; }
    j=i;
    j-=YSIZE;
    while (!IsBitSetBS(maze->out,j)) { dim++; j-=YSIZE; }
    if (dim<tunnel) { tunnel = dim; min_dim = YSIZE; }

    dim=1;
    j=i;
    j++;
    while (!IsBitSetBS(maze->out,j)) { dim++; j++; }
    j=i;
    j--;
    while (!IsBitSetBS(maze->out,j)) { dim++; j--; }
    if (dim<tunnel) { tunnel  = dim; min_dim = 1; }

    maze->Phys[i].tunnel = (USHORT)tunnel;
    maze->Phys[i].min_dim = min_dim;

    /* collect both dimensions stone tunnels */
    if (IsBitSetBS(maze->dead,i)) continue;
    tunnel = YSIZE*XSIZE;
    dim=1;
    j=i;
    while (  IsBitSetBS(maze->S[EAST],j)
           ||IsBitSetBS(maze->S[WEST],j+YSIZE)) { dim++; j+=YSIZE; }
    j=i;
    while (  IsBitSetBS(maze->S[WEST],j)
                       ||IsBitSetBS(maze->S[EAST],j-YSIZE)) { dim++; j-=YSIZE; }
    if (dim<tunnel) { tunnel = dim; min_dim = YSIZE; }

    dim=1;
    j=i;
    while (  IsBitSetBS(maze->S[NORTH],j)
                       ||IsBitSetBS(maze->S[SOUTH],j+1)) { dim++; j++; }
    j=i;
    while (  IsBitSetBS(maze->S[SOUTH],j)
                       ||IsBitSetBS(maze->S[NORTH],j-1)) { dim++; j--; }
    if (dim<tunnel) { tunnel = dim; min_dim = 1; }

  }
}

int Frozen2(MAZE *maze, int dir_in, PHYSID pos)
/* Is this stone fixed on the goal? */
/* pos is a goal square */
{

  int    dir, ret;
  PHYSID pos1, pos2;

  dir  = opdir[dir_in];
  pos1 = pos + DirToDiff[dir];
  pos2 = pos - DirToDiff[dir];

  /* if anything fixed on either side, this dir_in is fixed */
  if (   IsBitSetBS(maze->stones_done,pos1)
      || IsBitSetBS(maze->stones_done,pos2)) {
    return(YES);
  }

  /* if both squares on the sides in dir_in are dead, dir_in is fixed */
  if (IsBitSetBS(maze->dead,pos1) && IsBitSetBS(maze->dead,pos2)) {
    return(YES);
  }

  /* if stones are there, test if this stone is stuck */
  ret = NO;
  SetBitBS(maze->stones_done,pos);
  if (IsBitSetBS(maze->stone,pos1) && Frozen2(maze,dir,pos1))
    ret = YES;
  else if (IsBitSetBS(maze->stone,pos2) && Frozen2(maze,dir,pos2))
    ret = YES;

  UnsetBitBS(maze->stones_done,pos);
  return(ret);
}

int IsStoneDone(MAZE *maze, PHYSID moveto)
/* This routine sets stones_done with all the stones that are fixed by the
 * lat move that put a stone on moveto. We have to iteratively look at all
 * the different moves that might be effected, e.g. if there is a stone
 * against a wall, pushing one beside it will fix both, not just the one
 * moved. */
/* stone is already at destination square, now try to set stones_done */
/* return YES if stone is fixed */
{
  if (   Frozen2(maze,0,moveto)
      && Frozen2(maze,1,moveto)) {
    return(YES);
  } else {
    return(NO);
  }
}

void PropStonesDone(MAZE *maze, PHYSID pos)
/* propagate stones_done around pos, assuming pos is fixed */
{
  PHYSID pos2;
  int    dir;

  SetBitBS(maze->stones_done,pos);
  for (dir=NORTH; dir<=WEST; dir++) {
    pos2 = pos+DirToDiff[dir];
    if (    IsBitSetBS(maze->stone,pos2)
        && !IsBitSetBS(maze->stones_done,pos2)
        &&  IsStoneDone(maze,pos2)) {
      SetBitBS(maze->stones_done,pos2);
    }
  }
}

void SetAllStonesDone(MAZE *maze)
/* Set all the stones on goals that are fixed. Mark in stones_done. */
{
  int stonei;
  PHYSID pos;

  CopyBS(maze->stones_done,maze->out);
  for (stonei=0; stonei<maze->number_stones; stonei++) {
    pos = maze->stones[stonei].loc;
    if (   IsBitSetBS(maze->goal, pos)
        &&!IsBitSetBS(maze->stones_done, pos)
        && IsStoneDone(maze, pos) ) {
      PropStonesDone(maze, pos);
    }
  }
}
