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
#include "experiments.h"

/*******************************************************************************/
/* 									       */
/* 	MOVE will now include only stone pushes and is centered on that notion.*/
/*	This way from and to in MOVE and UNMOVE will have a different meaning  */
/* 	than in PATH							       */
/* 									       */
/*******************************************************************************/

#define SETMOVE(move,v,ifrom,ito,manp)\
	(move).move_dist =1; (move).from =ifrom; (move).last_over =ifrom;\
	(move).macro_id =0; (move).to =ito; (move).value =v; (move).man=manp;

int DirToDiff[8] = {1,YSIZE,-1,-YSIZE,1+YSIZE,-1+YSIZE,-1-YSIZE,1-YSIZE};
int  OppDir[8] = {2,3,0,1,6,7,4,5};
int NextDir[8] = {1,2,3,0,5,6,7,4};
int PrevDir[8] = {3,0,1,2,7,4,5,6};

int GenerateMoves(MAZE *maze, MOVE *moves)
{
	PHYSID pos;
	int i,dir;
	int number_moves = 0;
	BitString stones_done;

	CopyBS(stones_done,maze->stones_done);
	for (i=0; i<maze->number_stones; i++) {
		pos = maze->stones[i].loc;
		if (  (maze->PHYSstone[pos]<0)
		    ||(IsBitSetBS(stones_done,pos)))
			continue;
		SetBitBS(stones_done,pos);
		for (dir=NORTH; dir<=WEST; dir++) {
			if (   (IsBitSetBS( maze->reach,pos+DirToDiff[dir]))
			    && (IsBitSetBS(maze->S[OppDir[dir]],pos))
			    && (maze->PHYSstone[pos-DirToDiff[dir]]<0)) {
				SETMOVE(moves[number_moves],1,
				        pos,pos-DirToDiff[dir],
					pos+DirToDiff[dir]);
				if (!DeadLock(maze,moves+number_moves)) {
					if (SubMacro(maze,moves,&number_moves))
						goto END;
					number_moves++;
/* JS: Check this does not exceed MAX_MOVES */
				}
			}
		}
	}
END:
	moves[number_moves].to = ENDPATH;
	moves[number_moves].from = ENDPATH;
	return(number_moves);
}

static int compare_val(const void *m1, const void *m2) {
	return(((MOVE*)m2)->value - ((MOVE*)m1)->value);
}

static int compare_negval(const void *m1, const void *m2) {
/* secondary ordering by man move distance */
/* if( ( (MOVE *)m1 )->value == ( (MOVE *)m2 )->value && */
/*     !ISDUMMYMOVE( *(MOVE *)m1 ) && !ISDUMMYMOVE( *( MOVE *)m2 ) ) */
/*   return IdaInfo->IdaMaze->m_weights[ ( (MOVE *)m1 )->last_over ]->w */
/*     [ IdaInfo->IdaMaze->manpos ] - */
/*     IdaInfo->IdaMaze->m_weights[ ( (MOVE *)m2 )->last_over ]->w */
/*     [ IdaInfo->IdaMaze->manpos ]; */
	return(((MOVE*)m1)->value - ((MOVE*)m2)->value);
}

int NoMoveOrdering(int depth, int number_moves)
{
	return(number_moves);
}

int NewMoveOrdering(int depth, int number_moves)
{
	IDAARRAY *S;
	MAZE  *maze;
	MOVE  *m,*lmove;
	PHYSID goalpos;
	GROOM *groom;
	int    e;
	int    i,diff;
	DIST   dist,d;

	if (number_moves>1) {
 		lmove = depth?&(IdaInfo->IdaArray[depth-1].currentmove)
				:&DummyMove;
		S = &(IdaInfo->IdaArray[depth]);
		maze = IdaInfo->IdaMaze;
		for (i=0; i<number_moves; i++) {
			m = &(S->moves[i]);
			if (ISDUMMYMOVE(*m)) {
				m->value = -ENDPATH;
				continue;
			}
			/* Use the closest entrance to the goalll area the
			 * stone is in (if) as goalpos */
			goalpos = maze->goals[
				    maze->lbtable[
				      maze->PHYSstone[m->from]].goalidx].loc;
			if (maze->groom_index[m->from] >= 0 ) {
			    dist  = StoneDist(maze,m->from,goalpos);
			} else if (maze->groom_index[goalpos] >= 0 ) {
			    groom = maze->grooms + maze->groom_index[goalpos];
			    dist  = StoneDist(maze,m->from,goalpos);
			    for (e = 0; e < groom->n; e++) {
				d = StoneDist(maze,m->from,groom->locations[e]);
				if (dist > d) {
					dist = d;
					goalpos = groom->locations[e];
				}
			    }
			} else {
			    dist = MAXDIST;
			    for (e = 0; e < maze->number_goals; e++) {
				d = StoneDist(maze,m->from,maze->goals[e].loc);
				if (dist > d) {
					dist = d;
					goalpos = maze->goals[e].loc;
				}
			    }
			}
			m->value = dist;

			if (m->macro_id != 4) diff = m->value -
			      StoneDistManpos(maze,m->to,goalpos,m->from);
			else diff = m->value;
			if (diff>0) {
				if (lmove->to == m->from)
					m->value -= ENDPATH;
			} else {
				m->value -= diff*100;
			}
		}
		My_qsort(&(S->moves),number_moves,sizeof(MOVE),compare_negval);
	}
	return(number_moves);
}

int ManDistMoveOrdering(int depth, int number_moves)
{
	if (number_moves>1) {
		My_qsort(&(IdaInfo->IdaArray[depth].moves),
			number_moves,sizeof(MOVE),compare_val);
	}
	return(number_moves);
}

int MakeMove(MAZE *maze, MOVE *move, UNMOVE *ret, int targetpen)
/* this is a routine that makes a STONE move, not merely a man move like
 * DoMove. It will just put the man to the new location */
{
	int i;
	int recalculate;	/* should we recalculate the distances */
	int fixed;		/* stone moved into goal is fixed? */
	GMNODE **gmtree=NULL;

	ret->manfrom  = maze->manpos;
	ret->stonefrom= move->from;
	ret->stoneto  = move->to;
	ret->macro_id = move->macro_id;
	ret->move_dist= move->move_dist;
	CopyBS( ret->save_old_no_reach, maze->old_no_reach);
	CopyBS( ret->save_reach, maze->reach );
	CopyBS( ret->save_no_reach, maze->no_reach );
	ret->old_GMTree = NULL;
	maze->goal_sqto = -1;
	ret->old_s_distances = NULL;

	recalculate = YES;
	fixed	    = NO;
	CopyBS(ret->old_stones_done,maze->stones_done);
	if (maze->groom_index[move->to]>=0 && Options.mc_gm==1) {
		gmtree = &(maze->gmtrees[maze->groom_index[move->to]]);
		if (move->macro_id == 4) {
			/* this is the case when the goal macro worked */
			ret->old_GMTree = *gmtree;

			fixed = YES;
			/* set GMTree to next level down */
			i = 0;
			while (   (i<(*gmtree)->number_entries)
		       	&& ((*gmtree)->entries[i].goal_loc!=move->to)) i++;
			if (i<(*gmtree)->number_entries) {
				if ((*gmtree)->entries[i].new_distances==NO) {
					recalculate = NO;
				}

				(*gmtree) = (*gmtree)->entries[i].next;
			} else {
				(*gmtree) = NULL;
			}
		}
	}

	MANTO(maze,move->last_over);
	STONEFROMTO(maze,ret->stonefrom,ret->stoneto);
	UpdateHashKey(maze, ret);
	CopyBS(maze->old_no_reach,maze->no_reach);
        MarkReach( maze );
	if (IsBitSetBS(maze->goal,move->to)) {
	    /* Moving onto a goal, see if we should mark stones_done */
	    if (fixed || IsStoneDone(maze,move->to)) {
		    PropStonesDone(maze,move->to);
	    } else recalculate = NO;
	    /* Only if we know better from the gm-pre-calc, don't recalculate */
	    if (recalculate == YES && Options.lb_dd == YES) {
		GetNewDistances(maze,ret);
	    }
	}
	/* make sure we recalculate the lower bound for the new distances */
	if (ret->old_s_distances == NULL) {
		BetterUpdateLowerBound(maze, ret, targetpen);
	} else {
		BetterLowerBound(maze);
	}

	SR(Assert(maze->manpos>0,"MakeMove: manpos < 0!\n"));
	maze->currentmovenumber++;
	maze->g += ret->move_dist;
	return(1);
}

int UnMakeMove(MAZE *maze, UNMOVE *unmove, int targetpen)
{
	GMNODE **gmtree;
	int	 new_h;

	new_h = maze->h;
	MANTO(maze,unmove->manfrom);
	STONEFROMTO(maze,unmove->stoneto,unmove->stonefrom);

	if (unmove->old_GMTree != NULL) {
		Options.mc_gm = 1;
		gmtree = &(maze->gmtrees[maze->groom_index[unmove->stoneto]]);
		(*gmtree) = unmove->old_GMTree;
	}

	if (IsBitSetBS(maze->goal,unmove->stoneto)) {
		CopyBS(maze->stones_done,unmove->old_stones_done);
	}
	if (unmove->old_s_distances != NULL) {
		/* Make sure we restore the old weight tables */
		maze->s_distances = unmove->old_s_distances;
		maze->m_distances = unmove->old_m_distances;
		maze->connected   = unmove->old_connected;
		CopyBS(maze->one_way,unmove->old_one_way);
		BetterLowerBound(maze);
	} else {
		BetterUpdateLowerBound2(maze, unmove, targetpen);
	}

	UpdateHashKey(maze, unmove);
	CopyBS( maze->reach, unmove->save_reach );
	CopyBS( maze->no_reach, unmove->save_no_reach );
	CopyBS( maze->old_no_reach, unmove->save_old_no_reach);

	if (  ((unmove->macro_id==4) || (maze->goal_sqto==unmove->stoneto))
	    &&(maze->h - unmove->move_dist == new_h    /* optimal move */)) {
		/* This is either the start or a continuation of a goal move */
		maze->goal_sqto = unmove->stonefrom;
	} else maze->goal_sqto = -1;

	maze->currentmovenumber--;
	maze->g -= unmove->move_dist;
	return(0);
}

int DistToGoal(MAZE *maze, PHYSID start, PHYSID goal, PHYSID *last_over) {
/* pseudo-recursive function to see how many pushes are needed to get to
 * goal */

        static PHYSID stack[XSIZE*YSIZE];
        static PHYSID from[XSIZE*YSIZE];
        static PHYSID dist[XSIZE*YSIZE];
	static int s_visited[4][XSIZE*YSIZE];
        PHYSID pos,fro,old_man;
	char   old_stone;
        int    goal_dist, next_in, next_out, dir, dis;

	for (pos=0; pos<XSIZE*YSIZE; pos++) {
		for (dir=NORTH; dir<=WEST; dir++) {
			s_visited[dir][pos]=-1;
		}
	}

	/* set initial from to a position immediate to start on right side */
	for (dir=NORTH; dir<=WEST; dir++) {
		if (IsBitSetBS(maze->reach,start+DirToDiff[dir])) {
			from[0]  = start+DirToDiff[dir];
			break;
		}
	}
	old_man  = maze->manpos;
        stack[0] = start;
	dist[0]  = 0;
	old_stone= maze->PHYSstone[start];
	maze->PHYSstone[start]=-1;
        next_in  = 1;
        next_out = 0;
	goal_dist = -1;
        while (next_out < next_in) {
		fro = from[next_out];
                pos = stack[next_out];
                dis = dist[next_out++];
                if (maze->PHYSstone[pos] >= 0)
			continue;
		if (pos==goal) {
			*last_over = fro;
			goal_dist = dis;
			break;
		}
		dir = DiffToDir(fro-pos);
		if (s_visited[dir][pos] > 0) continue;

		/* set maze up to make the reach analysis for the man */
		MANTO(maze,fro);
		AvoidThisSquare = pos;
		MarkReach(maze);

		for (dir=NORTH; dir<=WEST; dir++) {
			if (IsBitSetBS(maze->reach,pos+DirToDiff[dir]))
				s_visited[dir][pos] = dis;
		}
		for (dir=NORTH; dir<=WEST; dir++) {
                	if (   IsBitSetBS(maze->S[dir],pos)
			    && IsBitSetBS(maze->reach, pos-DirToDiff[dir])) {
				from[next_in] = pos;
				dist[next_in] = dis + 1;
				stack[next_in++] = pos+DirToDiff[dir];
			}
		}
		dis = -1;
        }
	MANTO(maze,old_man);
	maze->PHYSstone[start]=old_stone;
	AvoidThisSquare = 0;
	MarkReach(maze);
	return(goal_dist);
}

/* Moves:
 * Fills in the reach char array with the possible locations the
 * man can move to. The reach char array stores Manhattan distances from
 * the man's starting position, and -1 if it is unreachable.
 */
void MovesImplOriginal(MAZE *maze, PHYSID *from, signed char *reach) {
  extern unsigned long long moves_while_counts;
  static PHYSID queue[ENDPATH];
  static PHYSID f[ENDPATH]; // This is also a queue of parents
  PHYSID pos;
  int next_in, next_out, dir;

  memset(reach, -1, XSIZE * YSIZE); // default to all unreachable
  queue[0] = maze->manpos;  // start at the current position
  f[0] = -1;
  next_in = 1;
  next_out = -1;
  while(++next_out < next_in) { // Loop until queue is empty

    pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos) continue;

    // while counts
    ++moves_while_counts;

    // get the parent's distance + 1, it will access reach[-1]...
    reach[pos] = reach[from[pos] = f[next_out]] + 1;

    for (dir = NORTH; dir <= WEST; dir++) {
      if (IsBitSetBS(maze->M[dir], pos)) { // Check if we can move this way
	      f[next_in] = pos;
	      queue[next_in] = pos + DirToDiff[dir];
        next_in++;
      }
    }
  }
}

/* Moves:
 * Fills in the reach char array with the possible locations the
 * man can move to. The reach char array stores Manhattan distances from
 * the man's starting position, and -1 if it is unreachable.
 */
void MovesImplLongBitString(MAZE *maze, PHYSID *from, signed char *reach) {
  extern unsigned long long moves_while_counts;
  static PHYSID queue[ENDPATH];
  static PHYSID f[ENDPATH]; // This is also a queue of parents
  PHYSID pos;
  int next_in, next_out, dir;

  memset(reach, -1, XSIZE * YSIZE); // default to all unreachable
  queue[0] = maze->manpos;  // start at the current position
  f[0] = -1;
  next_in = 1;
  next_out = -1;

#define IsBitSetLBS(a,p,dir) ((a)[(p) / 8] & (((BASETYPE)(1 << (dir))) << (((p) % 8) * 4)))

  while(++next_out < next_in) { // Loop until queue is empty

    pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos) continue;

    // while counts
    ++moves_while_counts;

    // get the parent's distance + 1, it will access reach[-1]...
    reach[pos] = reach[from[pos] = f[next_out]] + 1;

    for (dir = NORTH; dir <= WEST; dir++) {
      if (IsBitSetLBS(maze->Packed_M, pos, dir)) {
	      f[next_in] = pos;
	      queue[next_in] = pos + DirToDiff[dir];
        next_in++;
      }
    }
  }
}

/* Moves:
 * Fills in the reach char array with the possible locations the
 * man can move to. The reach char array stores Manhattan distances from
 * the man's starting position, and -1 if it is unreachable.
 */
void MovesImplUnrollFor(MAZE *maze, PHYSID *from, signed char *reach) {
  extern unsigned long long moves_while_counts;
  static PHYSID queue[ENDPATH];
  static PHYSID f[ENDPATH]; // This is also a queue of parents
  PHYSID pos;
  int next_in, next_out, dir;

  memset(reach, -1, XSIZE * YSIZE); // default to all unreachable
  queue[0] = maze->manpos;  // start at the current position
  f[0] = -1;
  next_in = 1;
  next_out = -1;
  while(++next_out < next_in) { // Loop until queue is empty

    pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos) continue;

    // while counts
    ++moves_while_counts;

    // get the parent's distance + 1, it will access reach[-1]...
    int parent = f[next_out];
    from[pos] = parent;
    reach[pos] = reach[parent] + 1;

#define CHECK_AND_PUSH(dir, pos, amount) \
    if (IsBitSetBS(maze->M[dir], pos)) { \
	      f[next_in] = pos; \
	      queue[next_in] = pos + amount; \
        next_in++; \
    }

    CHECK_AND_PUSH(NORTH, pos, 1)
    CHECK_AND_PUSH(EAST, pos, YSIZE)
    CHECK_AND_PUSH(SOUTH, pos, -1)
    CHECK_AND_PUSH(WEST, pos, -YSIZE)
  }
}

/* Moves:
 * Fills in the reach char array with the possible locations the
 * man can move to. The reach char array stores Manhattan distances from
 * the man's starting position, and -1 if it is unreachable.
 */
void MovesImplUnrollWhile(MAZE *maze, PHYSID *from, signed char *reach) {
  extern unsigned long long moves_while_counts;
  static PHYSID queue[ENDPATH];
  static PHYSID f[ENDPATH]; // This is also a queue of parents
  int next_in, next_out, dir;
  
  PHYSID first_pos, second_pos;

#define CHECK_AND_PUSH(dir, pos, amount) \
    if (IsBitSetBS(maze->M[dir], pos)) { \
	      f[next_in] = pos; \
	      queue[next_in] = pos + amount; \
        next_in++; \
    }

  memset(reach, -1, XSIZE * YSIZE); // default to all unreachable
  queue[0] = maze->manpos;  // start at the current position
  f[0] = -1;
  next_in = 1;
  next_out = -1;
  while(1) {
    if (!(++next_out < next_in)) break;
    first_pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (reach[first_pos] >= 0 || maze->PHYSstone[first_pos] >= 0 || AvoidThisSquare == first_pos) continue;

    // while counts
    ++moves_while_counts;

    // get the parent's distance + 1, it will access reach[-1]...
    int first_parent = f[next_out];
    from[first_pos] = first_parent;
    reach[first_pos] = reach[first_parent] + 1;

    CHECK_AND_PUSH(NORTH, first_pos, 1)
    CHECK_AND_PUSH(EAST, first_pos, YSIZE)
    CHECK_AND_PUSH(SOUTH, first_pos, -1)
    CHECK_AND_PUSH(WEST, first_pos, -YSIZE)

    if (!(++next_out < next_in)) break;
    second_pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (reach[second_pos] >= 0 || maze->PHYSstone[second_pos] >= 0 || AvoidThisSquare == second_pos) continue;

    // while counts
    ++moves_while_counts;

    // get the parent's distance + 1, it will access reach[-1]...
    int second_parent = f[next_out];
    from[second_pos] = second_parent;
    reach[second_pos] = reach[second_parent] + 1;

    CHECK_AND_PUSH(NORTH, second_pos, 1)
    CHECK_AND_PUSH(EAST, second_pos, YSIZE)
    CHECK_AND_PUSH(SOUTH, second_pos, -1)
    CHECK_AND_PUSH(WEST, second_pos, -YSIZE)
  }
}

/* Moves:
 * Fills in the reach char array with the possible locations the
 * man can move to. The reach char array stores Manhattan distances from
 * the man's starting position, and -1 if it is unreachable.
 */
void MovesImplLoadLBS(MAZE *maze, PHYSID *from, signed char *reach) {
  extern unsigned long long moves_while_counts;
  static PHYSID queue[ENDPATH];
  static PHYSID f[ENDPATH]; // This is also a queue of parents
  PHYSID pos;
  int next_in, next_out, dir;

  memset(reach, -1, XSIZE * YSIZE); // default to all unreachable
  queue[0] = maze->manpos;  // start at the current position
  f[0] = -1;
  next_in = 1;
  next_out = -1;
  while(++next_out < next_in) { // Loop until queue is empty

    pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos) continue;

    // while counts
    ++moves_while_counts;

    // get the parent's distance + 1, it will access reach[-1]...
    int parent = f[next_out];
    from[pos] = parent;
    reach[pos] = reach[parent] + 1;

#define CHECK_AND_PUSH_2(cond, pos, amount) \
    if (cond) { \
	      f[next_in] = pos; \
	      queue[next_in] = pos + amount; \
        next_in++; \
    }

#define LoadFourBitsLBS(a, p) ((((a)[(p) / 8]) >> (((p) % 8) * 4)) & 15)
    PHYSID fourBits = LoadFourBitsLBS(maze->Packed_M, pos);

    CHECK_AND_PUSH_2(fourBits & 1, pos, 1)
    CHECK_AND_PUSH_2(fourBits & 2, pos, YSIZE)
    CHECK_AND_PUSH_2(fourBits & 4, pos, -1)
    CHECK_AND_PUSH_2(fourBits & 8, pos, -YSIZE)
  }
}

/* Moves:
 * Fills in the reach char array with the possible locations the
 * man can move to. The reach char array stores Manhattan distances from
 * the man's starting position, and -1 if it is unreachable.
 */
void MovesImplDoNotPushPreviousOne(MAZE *maze, PHYSID *from, signed char *reach) {
  extern unsigned long long moves_while_counts;
  static PHYSID queue[ENDPATH];
  static PHYSID f[ENDPATH]; // This is also a queue of parents
  PHYSID pos;
  int next_in, next_out, dir;

  memset(reach, -1, XSIZE * YSIZE); // default to all unreachable
  queue[0] = maze->manpos;  // start at the current position
  f[0] = -1;
  next_in = 1;
  next_out = -1;

  {
    ++next_out;
    pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (!(reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos))  {
      // while counts
      ++moves_while_counts;

      // get the parent's distance + 1, it will access reach[-1]...
      int parent = f[next_out];
      from[pos] = parent;
      reach[pos] = reach[parent] + 1;
      CHECK_AND_PUSH(NORTH, pos, 1)
      CHECK_AND_PUSH(EAST, pos, YSIZE)
      CHECK_AND_PUSH(SOUTH, pos, -1)
      CHECK_AND_PUSH(WEST, pos, -YSIZE)
    }
  }

  while(++next_out < next_in) { // Loop until queue is empty
    pos = queue[next_out]; // Grab the next location from the queue

    // check whether it has been reached or there is a stone
    if (reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos) continue;

    // while counts
    ++moves_while_counts;

    // get the parent's distance + 1, it will access reach[-1]...
    int parent = f[next_out];
    from[pos] = parent;
    reach[pos] = reach[parent] + 1;

#define CHECK_AND_PUSH_3(dir, pos, amount) \
    if (IsBitSetBS(maze->M[dir], pos)) { \
        f[next_in] = pos; \
	      queue[next_in] = pos + amount; \
        next_in++; \
    }

    switch(parent-pos) {
      case 1:
        CHECK_AND_PUSH_3(EAST, pos, YSIZE)
        CHECK_AND_PUSH_3(SOUTH, pos, -1)
        CHECK_AND_PUSH_3(WEST, pos, -YSIZE)
        break;
      case YSIZE:
        CHECK_AND_PUSH_3(NORTH, pos, 1)
        CHECK_AND_PUSH_3(SOUTH, pos, -1)
        CHECK_AND_PUSH_3(WEST, pos, -YSIZE)
        break;
      case -1:
        CHECK_AND_PUSH_3(NORTH, pos, 1)
        CHECK_AND_PUSH_3(EAST, pos, YSIZE)
        CHECK_AND_PUSH_3(WEST, pos, -YSIZE)
        break;
      case -YSIZE:
        CHECK_AND_PUSH_3(NORTH, pos, 1)
        CHECK_AND_PUSH_3(EAST, pos, YSIZE)
        CHECK_AND_PUSH_3(SOUTH, pos, -1)
        break;
    }
  }
}

#include<assert.h>
/* #define ULL unsigned __int128 */
/* #define NUM_OF_SHORTS 8 */
#define ULL unsigned long long
#define NUM_OF_SHORTS 4
/* Moves:
 * Fills in the reach char array with the possible locations the
 * man can move to. The reach char array stores Manhattan distances from
 * the man's starting position, and -1 if it is unreachable.
 */
void MovesImpl(MAZE *maze, PHYSID *from, signed char *reach) {
  extern unsigned long long moves_while_counts;
  /* static PHYSID queue[ENDPATH]; */
  /* static PHYSID f[ENDPATH]; // This is also a queue of parents */

  static ULL queue[ENDPATH/4];
  static ULL f[ENDPATH/4]; // This is also a queue of parents
  ULL cur_queue, cur_f;

  PHYSID pos;
  PHYSID cur_in, cur_out, next_in, next_out, sz;

  memset(reach, -1, XSIZE * YSIZE); // default to all unreachable
  cur_queue = maze->manpos;
  cur_f = (PHYSID)-1;

  cur_in = 1;
  cur_out = -1;

  next_in = 0;
  next_out = -1;

#define PHYSID_MASK (ULL)(0xffff)
#define CHECK_AND_PUSH_4(dir, pos, amount) \
    if (IsBitSetBS(maze->M[dir], pos)) { \
      if (cur_in < NUM_OF_SHORTS) { \
        cur_f = (cur_f & (~(PHYSID_MASK << (cur_in << 4)))) | ((ULL)pos << (cur_in << 4)) ; \
        cur_queue = (cur_queue & (~(PHYSID_MASK << (cur_in << 4)))) | (((ULL)pos + amount) << (cur_in << 4)) ; \
        cur_in++; \
      } else { \
        f[next_in / NUM_OF_SHORTS] = (f[next_in / NUM_OF_SHORTS] & (~(PHYSID_MASK << ((next_in % NUM_OF_SHORTS) << 4)))) | ((ULL)pos << ((next_in % NUM_OF_SHORTS) << 4)) ; \
        queue[next_in / NUM_OF_SHORTS] = (queue[next_in / NUM_OF_SHORTS] & (~(PHYSID_MASK << ((next_in % NUM_OF_SHORTS) << 4)))) | ((ULL)(pos + amount) << ((next_in % NUM_OF_SHORTS) << 4)) ; \
        next_in++; \
      } \
    }
      /* switch(cur_in) { \ */
        /* case 0: \ */
          /* cur_f = (cur_f & (~(PHYSID_MASK))) | ((ULL)pos) ; \ */
          /* cur_queue = (cur_queue & (~(PHYSID_MASK))) | (((ULL)pos + amount)) ; \ */
          /* cur_in++; \ */
          /* break; \ */
        /* case 1: \ */
          /* cur_f = (cur_f & (~(PHYSID_MASK << (1 << 4)))) | ((ULL)pos << (1 << 4)) ; \ */
          /* cur_queue = (cur_queue & (~(PHYSID_MASK << (1 << 4)))) | (((ULL)pos + amount) << (1 << 4)) ; \ */
          /* cur_in++; \ */
        /* case 2: \ */
          /* cur_f = (cur_f & (~(PHYSID_MASK << (2 << 4)))) | ((ULL)pos << (2 << 4)) ; \ */
          /* cur_queue = (cur_queue & (~(PHYSID_MASK << (2 << 4)))) | (((ULL)pos + amount) << (2 << 4)) ; \ */
          /* cur_in++; \ */
        /* case 3: \ */
          /* cur_f = (cur_f & (~(PHYSID_MASK << (3 << 4)))) | ((ULL)pos << (3 << 4)) ; \ */
          /* cur_queue = (cur_queue & (~(PHYSID_MASK << (3 << 4)))) | (((ULL)pos + amount) << (3 << 4)) ; \ */
          /* cur_in++; \ */
        /* case 4: \ */
          /* f[next_in / 4] = (f[next_in / 4] & (~(PHYSID_MASK << ((next_in % 4) << 4)))) | ((ULL)pos << ((next_in % 4) << 4)) ; \ */
          /* queue[next_in / 4] = (queue[next_in / 4] & (~(PHYSID_MASK << ((next_in % 4) << 4)))) | ((ULL)(pos + amount) << ((next_in % 4) << 4)) ; \ */
          /* next_in++; \ */
          /* break; \ */
      /* } \ */

      /* if (cur_in < 4) { */
        /* ULL multi_pos = pos; */
        /* multi_pos |= multi_pos << 16; */
        /* multi_pos |= multi_pos << 32; */
        /* cur_f = (cur_f & (~0ULL >> ((4 - cur_in) << 4))) | (multi_pos << (cur_in << 4)); */
      /* } */

  {
    ++cur_out;
    pos = (cur_queue >> (cur_out << 4)) & PHYSID_MASK;

    // check whether it has been reached or there is a stone
    if (!(reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos))  {
      // while counts
      ++moves_while_counts;

      // get the parent's distance + 1, it will access reach[-1]...
      int parent = (cur_f >> (cur_out << 4)) & PHYSID_MASK;
      from[pos] = parent;
      reach[pos] = reach[parent] + 1;

      CHECK_AND_PUSH_4(NORTH, pos, 1)
      CHECK_AND_PUSH_4(EAST, pos, YSIZE)
      CHECK_AND_PUSH_4(SOUTH, pos, -1)
      CHECK_AND_PUSH_4(WEST, pos, -YSIZE)
    }
  }

  while(++cur_out < cur_in) { // Loop until queue is empty
    pos = (cur_queue >> (cur_out << 4)) & PHYSID_MASK;

    // check whether it has been reached or there is a stone
    if (reach[pos] >= 0 || maze->PHYSstone[pos] >= 0 || AvoidThisSquare == pos) goto END_OF_WHILE;

    // while counts
    ++moves_while_counts;

    int parent = (cur_f >> (cur_out << 4)) & PHYSID_MASK;
    from[pos] = parent;
    reach[pos] = reach[parent] + 1;

    switch(parent-pos) {
      case 1:
        CHECK_AND_PUSH_4(EAST, pos, YSIZE)
        CHECK_AND_PUSH_4(SOUTH, pos, -1)
        CHECK_AND_PUSH_4(WEST, pos, -YSIZE)
        break;
      case YSIZE:
        CHECK_AND_PUSH_4(NORTH, pos, 1)
        CHECK_AND_PUSH_4(SOUTH, pos, -1)
        CHECK_AND_PUSH_4(WEST, pos, -YSIZE)
        break;
      case -1:
        CHECK_AND_PUSH_4(NORTH, pos, 1)
        CHECK_AND_PUSH_4(EAST, pos, YSIZE)
        CHECK_AND_PUSH_4(WEST, pos, -YSIZE)
        break;
      case -YSIZE:
        CHECK_AND_PUSH_4(NORTH, pos, 1)
        CHECK_AND_PUSH_4(EAST, pos, YSIZE)
        CHECK_AND_PUSH_4(SOUTH, pos, -1)
        break;
      default:
        assert(0 && "Should not reach here.");
    }

END_OF_WHILE:
    if (cur_out == NUM_OF_SHORTS - 1) {
      if (next_out + NUM_OF_SHORTS < next_in) {
        cur_in  = NUM_OF_SHORTS;
        next_out += NUM_OF_SHORTS;
      } else if (next_out + 1 < next_in) {
        cur_in  = next_in - 1 - next_out;
        next_in = ((next_in + NUM_OF_SHORTS - 1) / NUM_OF_SHORTS) * NUM_OF_SHORTS;
        next_out = next_in - 1;
      } else {
        cur_in  = 0;
      }
      cur_out = -1;
      cur_queue = queue[next_out / NUM_OF_SHORTS];
      cur_f = f[next_out / NUM_OF_SHORTS];
    }
  }
}

/*      U
 *     LCR
 *      B
 *
 *  Fixed Map of occupations (Max 320, 4-bit)
 *
 *  Queue of positions (Max 160, 9-bit)
 *  Queue of parents (Max 320, 2-bit)
 *
 *  Output: Map of parents (Max 320, 2-bit)
 *  Output: Map of distances (Max 320, 6-bit)
 *
 *  Input:
 *  Map with manpos, walls, and stones
 *
 *  Output:
 *  Distances and Parents of each position
 *
 *  1. Pop position queue (increment end + load) -> 2 cycles
 *  2. Check visited (load) -> 1 cycle
 *  3. Check stone (load) -> 1 cycle
 *  4. Update Map of parents (load and store) -> 2 cycle
 *  5. Update Map of distances (load, increment, and store) -> 3 cycles
 *  6. Try 4 directions: 4 x 5 cycles = 20 cycles
 *     - check occupations (load) -> 1 cycle
 *     - update Queue of parents (store) -> 1 cycle
 *     - push into Queue of positions (add and store) -> 2 cycles
 *     - increment front (increment) -> 1 cycle
 *
 *  Total 29 cycles.
 */

#include<assert.h>
void Moves(MAZE *maze, PHYSID *from, signed char *reach) {
  /* a wrapper for counting number of cycles in MovesImpl() */
  extern unsigned long long moves_cycles;
  extern unsigned long long moves_counts;

  ++moves_counts;
  unsigned long long cnt = rdtsc();

  /* MovesImpl(maze, from, reach); */
  MovesImplDoNotPushPreviousOne(maze, from, reach);

  moves_cycles += rdtsc() - cnt;
}

/* generate a path that does not touch any of the squares in shadows */
void Moves2(MAZE *maze, PHYSID *from, signed char *reach, BitString shadows )
{
  static PHYSID stack[ ENDPATH ];
  static PHYSID f[ ENDPATH ];
  PHYSID pos;
  int next_in, next_out, dir;

  memset( reach, -1, XSIZE * YSIZE );
  stack[ 0 ] = maze->manpos;
  f[ 0 ] = 0;
  from[ 0 ] = -1;
  next_in  = 1;
  next_out = -1;
  while( ++next_out < next_in ) {
    pos = stack[ next_out ];
    if( reach[ pos ] >= 0 ) continue;
    if( maze->PHYSstone[ pos ] >= 0 ) continue;
    if( AvoidThisSquare == pos ) continue;
    if( IsBitSetBS( shadows, pos ) ) continue;
    reach[ pos ] = reach[ from[ pos ] = f[ next_out ] ] + 1;
    for( dir = NORTH; dir <= WEST; dir++ )
      if( IsBitSetBS( maze->M[ dir ], pos ) ) {
	f[ next_in ] = pos;
	stack[ next_in++ ] = pos + DirToDiff[ dir ];
      }
  }
}

void GenAllSquares( PHYSID pos, PHYSID *from, BitString all_squares )
{
  Set0BS( all_squares );
  SetBitBS( all_squares, pos );
  while( from[ pos ] > 0 ) {
    pos = from[ pos ];
    SetBitBS( all_squares, pos );
  }
}

void PushesMoves(MAZE *maze, PHYSID start, PHYSID goal,
		 int *pushes, int *moves,
		 BitString stone_squares, BitString man_squares)
{

  static PHYSID stack[XSIZE*YSIZE];
  static PHYSID from[XSIZE*YSIZE];
  static PHYSID mand[XSIZE*YSIZE];
  static PHYSID n_pu[XSIZE*YSIZE];
  static PHYSID n_mo[XSIZE*YSIZE];
  static PHYSID movefrom[ XSIZE * YSIZE ];
  static BitString s_squares[XSIZE*YSIZE];
  static BitString m_squares[XSIZE*YSIZE];
  static BitString all_square;
  static int s_visited[XSIZE*YSIZE];
  static signed char reach[ XSIZE * YSIZE ];
  PHYSID pos,fro,old_man;
  char   old_stone;
  int    next_in, next_out, dir;

  memset( s_visited, 0, sizeof( int ) * XSIZE * YSIZE );
  memset( n_pu, 0, sizeof( PHYSID ) * XSIZE * YSIZE );
  memset( n_mo, 0, sizeof( PHYSID ) * XSIZE * YSIZE );

  old_man  = maze->manpos;
  from[0]  = maze->manpos;
  n_pu[maze->manpos]  = -1;
  n_mo[maze->manpos]  = -1;
  stack[0] = start;
  mand[0]  = 0;
  Set0BS(s_squares[0]);
  old_stone= maze->PHYSstone[start];
  maze->PHYSstone[start]=-1;
  next_in  = 1;
  next_out = 0;
  AvoidThisSquare = start;
  while (next_out < next_in) {
    fro = from[next_out];
    pos = stack[next_out++];
    if (maze->PHYSstone[pos] >= 0)
      continue;
    if (s_visited[pos] ) continue;
    s_visited[pos] = 1;
    n_pu[pos]      = n_pu[fro]+1;
    n_mo[pos]      = n_mo[fro]+ mand[next_out-1] +1;
    if (pos==goal) break;

    /* set maze up to make the reach analysis for the man */
    MANTO(maze,fro);
    AvoidThisSquare = pos;
    Moves(maze,movefrom,reach);

    for (dir=NORTH; dir<=WEST; dir++) {
      if (   IsBitSetBS(maze->S[dir],pos)
	     && reach[pos-DirToDiff[dir]]>=0) {
	mand[next_in] = reach[pos-DirToDiff[dir]];
	CopyBS(m_squares[next_in], m_squares[next_out-1]);
	GenAllSquares( pos - DirToDiff[ dir ], movefrom, all_square );
	BitOrEqBS( m_squares[next_in], all_square );
	from[next_in] = pos;
	CopyBS(s_squares[next_in], s_squares[next_out-1]);
	SetBitBS(s_squares[next_in],pos);
	stack[next_in++] = pos +DirToDiff[dir];
      }
    }
  }
  MANTO(maze,old_man);
  maze->PHYSstone[start]=old_stone;
  AvoidThisSquare = 0;
  MarkReach(maze);
  *pushes = n_pu[goal];
  *moves  = n_mo[goal];
  CopyBS(stone_squares,s_squares[next_out-1]);
  SetBitBS(stone_squares,pos);
  CopyBS(man_squares,m_squares[next_out-1]);
  SetBitBS(man_squares,fro);
}

void PushesMoves2(MAZE *maze, PHYSID start, PHYSID goal,
		 int *pushes, int *moves,
		 BitString stone_squares, BitString man_squares)
{
  static PHYSID stack[XSIZE*YSIZE];
  static PHYSID from[XSIZE*YSIZE];
  static PHYSID mand[XSIZE*YSIZE];
  static PHYSID n_pu[XSIZE*YSIZE];
  static PHYSID n_mo[XSIZE*YSIZE];
  static PHYSID movefrom[ XSIZE * YSIZE ];
  static BitString s_squares[XSIZE*YSIZE];
  static BitString m_squares[XSIZE*YSIZE];
  static BitString all_square;
  static int s_visited[XSIZE*YSIZE];
  static signed char reach[ XSIZE * YSIZE ];
  PHYSID pos,fro,old_man;
  char   old_stone;
  int    next_in, next_out, dir;

  memset( s_visited, 0, sizeof( int ) * XSIZE * YSIZE );
  memset( n_pu, 0, sizeof( PHYSID ) * XSIZE * YSIZE );
  memset( n_mo, 0, sizeof( PHYSID ) * XSIZE * YSIZE );

  old_man = maze->manpos;
  from[ 0 ] = maze->manpos;
  n_pu[ maze->manpos ] = -1;
  n_mo[ maze->manpos ] = -1;
  stack[ 0 ] = start;
  mand[ 0 ] = 0;
  Set0BS( s_squares[ 0 ] );
  old_stone = maze->PHYSstone[ start ];
  maze->PHYSstone[ start ] = -1;
  next_in = 1;
  next_out = 0;
  AvoidThisSquare = start;
  while( next_out < next_in ) {
    fro = from[ next_out ];
    pos = stack[ next_out++ ];
    if( maze->PHYSstone[ pos ] >= 0 )
      continue;
    if( s_visited[ pos ] ) continue;
    s_visited[ pos ] = 1;
    n_pu[ pos ] = n_pu[ fro ] + 1;
    n_mo[ pos ] = n_mo[ fro ] + mand[ next_out - 1 ] + 1;
    if( pos == goal )
      break;

    /* set maze up to make the reach analysis for the man */
    MANTO( maze, fro );
    AvoidThisSquare = pos;
    Moves2( maze, movefrom, reach, IdaInfo->shadow_stones );

    /* try to avoid shadow stones */
    for( dir = NORTH; dir <= WEST; dir++ ) {
      if( IsBitSetBS( maze->S[ dir ], pos ) &&
	  reach[ pos - DirToDiff[ dir ] ] >=0 &&
	  !s_visited[ pos + DirToDiff[ dir ] ] ) {
	mand[ next_in ] = reach[ pos - DirToDiff[ dir ] ];
	CopyBS( m_squares[ next_in ], m_squares[ next_out - 1 ] );
	GenAllSquares( pos - DirToDiff[ dir ], movefrom, all_square );
	BitOrEqBS( m_squares[ next_in ], all_square );
	from[ next_in ] = pos;
	CopyBS( s_squares[ next_in ], s_squares[ next_out - 1 ] );
	SetBitBS( s_squares[ next_in ], pos );
	stack[ next_in++ ] = pos + DirToDiff[ dir ];
	s_visited[ pos + DirToDiff[ dir ] ] = 1;
      }
    }
  }
  MANTO( maze, old_man );
  maze->PHYSstone[ start ] = old_stone;
  AvoidThisSquare = 0;
  MarkReach( maze );
  *pushes = n_pu[ goal ];
  *moves = n_mo[ goal ];
  CopyBS( stone_squares, s_squares[ next_out - 1 ] );
  SetBitBS( stone_squares, pos );
  CopyBS( man_squares, m_squares[ next_out - 1 ] );
  SetBitBS( man_squares, fro );
}

int ValidSolution(MAZE *maze, MOVE *solution) {

	int number_pushes, number_moves, i;
	int p,m;
	UNMOVE unmove;
	BitString stone,man;

	number_pushes = number_moves = 0;
	i = 0;
	while (!ISDUMMYMOVE(solution[i])) {
		p = m = 0;
		PushesMoves(maze,solution[i].from,solution[i].to,
			    &p,&m,stone,man);
		number_pushes += p;
		number_moves  += m;
		MakeMove(maze,&(solution[i]),&unmove, ENDPATH);
		i++;
	}
	Mprintf(0,"Moves: %i, Pushes: %i, Treedepth: %i\n",
		number_moves,number_pushes,i);
	if (maze->h!=0)
		return(0);
	else return(number_pushes);

}


int DiffToDir(int diff)
{
	switch (diff) {
	case -1:
		return(SOUTH);
	case  1:
		return(NORTH);
	case -YSIZE:
		return(WEST);
	case YSIZE:
		return(EAST);
	case +1+YSIZE:
		return(NORTHEAST);
	case -1+YSIZE:
		return(SOUTHEAST);
	case -1-YSIZE:
		return(SOUTHWEST);
	case +1-YSIZE:
		return(NORTHWEST);
	default:
		return(NODIR);
	}
}
