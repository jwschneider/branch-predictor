// tagged-hybrid_predictor.h
// The tagged hybrid predictor uses cascading prediction tables
// indexed by a hash of the history and the pc.
// Current hash implementation is a simple xor hash; TBU.
#include <vector>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <math.h>
using std::vector;
using std::cout;
using std::endl;

// This function is for printing binary values, delete before submission
void printBits(size_t const size, void const * const ptr)
{
  unsigned char *b = (unsigned char*) ptr;
  unsigned char byte;
  int i, j;
  for (i=size-1;i>=0;i--)
  {
    for (j=7;j>=0;j--)
    {
      byte = (b[i] >> j) & 1;
      printf("%u", byte);
    }
  }
}


class my_update : public branch_update {
public:
  // Instead of keeping the table and index, we keep the
  // provider, prov, and the alternate, alt
  unsigned int prov;
  unsigned int alt;
};

class my_predictor : public branch_predictor {
public:
//const unsigned int LIMIT_FACTOR = 15000; // when to wipe table
const unsigned int TABLE_BITS	= 12;     // 2^12(rows per table) * 2^3(tables) * 2^3(bits per row)
const unsigned int HISTORY_LEN = 64;    // is 2^18 Kibits = 2^15 KiB
const unsigned int TAG_LEN = 6;
const unsigned int TABLE_CT = 8;
const bool UPDATE_ALT_ON_HIT = true;    // I tested this once and it seems to work better 
                                        // with this on

	my_update u;
	branch_info bi;
	unsigned long long hist;    // I made history a long long (64 bits on most machines) 
                              // so that we could have 8 tables

	vector<unsigned int *> pred; 


  // Initialize the tables to a particular value
  // Weakly not taken preforms a tiny bit better than weakly taken
	my_predictor (void) : hist(0) { 
		for (unsigned int i = 0; i < TABLE_CT; i++) {
			unsigned int* tbl = (unsigned int*) malloc((1 << TABLE_BITS) * sizeof(unsigned int));
		    for (int i = 0; i < (1<<TABLE_BITS); i++) {
		      *(tbl + i) = 1 << (TAG_LEN);
		    }
				pred.push_back(tbl);
		  } 
	}

  // Updates the prediction of a table element, preserving the tag
  void update_prediction(unsigned int &elem, bool taken) {
    unsigned int pred = elem >> TAG_LEN;
    if (taken && (pred < 3))
      pred++;
    else if (!taken && (pred > 0)) 
      pred--;
    elem = ((elem) & ((1<<TAG_LEN)-1)) | (pred << TAG_LEN);
  }

  // Steps the history counter
  void advance_history(bool taken) {
    if (HISTORY_LEN < sizeof(hist) * 8)
      hist = ((hist << 1) | taken) & ((1<< HISTORY_LEN) - 1);
    else
      hist = (hist << 1) | taken;
  }

  // This is the part that was killing us. We two instances of a branch with
  // different histories to map to different indexes in the same table, so that
  // both can exist in the table at the same time. xor is a fine hash function,
  // but the problem is that we were hashing address with history and then only
  // looking at the bottom 12 bits of it (since we have to fit in the table).
  // That meant that even though two addresses had different histories, if they
  // were at least the same in the bottom 12 bits, then the two instances of
  // the branch would map to the same place in the highest table, causing
  // a ping pong effect. What i've done below is folded the history into the
  // address. If the size of the history that we're looking to hash is larger
  // than the number of rows in the table, first we hash the lower bits into
  // the address, then we slide history down and hash more bits into the
  // address. This way if a branch has two instances that match in history even
  // up to the 63rd bit but differ in the 64th, they will map to different
  // indices.
  unsigned int table_index(unsigned int address, unsigned int table_ct) {
    unsigned int accumulator = address;
    unsigned int batch = (unsigned int) gp(table_ct);
    unsigned long long hist_copy = hist;
    if (batch > TABLE_BITS) {
      unsigned int ct = TABLE_BITS;
      while (ct < batch) {
        accumulator ^= (((1<<TABLE_BITS)-1) & hist_copy);
        hist_copy >>= std::min(TABLE_BITS, batch - ct);
        ct += std::min(TABLE_BITS, batch - ct);
      }
    } else 
      accumulator ^= (((1<<batch)-1) & hist);
    return accumulator & ((1<<TABLE_BITS)-1);
  }

  // Returns the i'th element of the geometric progression
  // We are using to select history length
  // Note the progression goes 0, 1, 2, 4, ...
  // So that for 8 tables we get 0 -> 64
  unsigned long long gp(int i) {
    if (i <= 0)
      return 0;
    else
      return pow(2, i - 1);
  }

	branch_update *predict (branch_info &b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
        u.alt = 0;
        u.prov = 0;
        // This loop sets prov to be the table with the longest matching
        // history and sets alt to be table with the second longest matching
        // history
			  for (unsigned int t_count = 1; t_count < TABLE_CT; t_count++) {
	        	if ((*(pred[t_count] + (table_index(b.address, t_count) & ((1<<TABLE_BITS) - 1))) 
                 & ((1<<TAG_LEN) - 1)) 
                 ==	(b.address & ((1<<TAG_LEN) - 1))) {
              u.alt = u.prov;
              u.prov = t_count;
				    }
			  }
        // If the provider prediction is weak, then we use the alternate
        // prediction instead
        unsigned int prov_pred = *(pred[u.prov] + table_index(b.address, u.prov)) >> TAG_LEN;
        if ((prov_pred == 3) || (prov_pred == 0))
          u.direction_prediction (prov_pred >> 1);
        else
          u.direction_prediction (*(pred[u.alt] + table_index(b.address, u.alt)) >> (TAG_LEN + 1));
		} else {
			u.direction_prediction (true);                  
		}
		u.target_prediction (0);                          
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {        
			my_update* y = (my_update*) u;
      // These are the predictions given by the provider and alternate
      unsigned int &prov_pred = *(pred[y->prov] + table_index(bi.address, y->prov));
      unsigned int &alt_pred = *(pred[y->alt] + table_index(bi.address, y->alt));
      // This is a really useful list of things to print when debugging
      // Delete before submission but please dont delete until we're
      // sure we have the correct implementation
      // It follows the trace of one address in 164, showing how it settles
      // itself into the table. If you're interested, run predict 164 > logfile
      // and watch the trace
      //  if (bi.address == 134515682) {
         // printf("Prov:%d\tIndex: ", y->prov);
         // unsigned int prov_index = table_index(bi.address, y->prov);
         // printBits(sizeof(unsigned int), &prov_index);
         // printf("\tPred: %d", prov_pred >> TAG_LEN);
         // printf("\tAlt:%d\tIndex: ", y->alt);
         // unsigned int alt_index = table_index(bi.address, y->alt);
         // printBits(sizeof(unsigned int), &alt_index);
         // printf("\tPred: %d", alt_pred >> TAG_LEN);
         // printf("\tTaken: %d\tHist: ", taken);
         // printBits(sizeof(hist), &hist);
         // puts("");
       // }
			//printf ("%d\t%d\t%d\t%d\n", y->table, y->index, prediction, taken);
			if (taken == y->direction_prediction()) {
        update_prediction(prov_pred, taken);
        // We can choose to update the alternate on a hit if we want,
        // this is something we would do if we add use bits
        if (UPDATE_ALT_ON_HIT)
          update_prediction(alt_pred, taken);
			} else {
        // Also useful for debugging the same trace
        // if (bi.address == 134515682)
          // printf("Mispredict!\n");
        // Another design change I made was that if we miss on the last table,
        // we do not go about allocating a new entry. Instead we just treat it
        // like a simple two-bit predictor, bumping the count up or down
        if (y->prov < TABLE_CT - 1) {
          srand(1);
          int rNum = rand()%2; 
          unsigned int tbl_ct = y->prov; 
          if (rNum) {
            // rNum is odd, update table i + 1
            tbl_ct = (y->prov + 1);
          } else {
            // if rNum is even, update either i+2, i+3
            rNum = rand()%2;
            if (rNum) {
              // if rNUm is odd, update table i+2
              tbl_ct = (y->prov + 2);
            } else {
              // if rNum is even, update table i+3
              tbl_ct = (y->prov + 3);
            }
          }
          if (tbl_ct >= TABLE_CT) {
            // if our adjustment has caused us to go out-of-bounds
            tbl_ct = TABLE_CT - 1;
          }
          // We allocate a new entry in the chosen table to weakly taken
          // or weakly not taken
          *(pred[tbl_ct] + table_index(bi.address, tbl_ct)) =
             ((((unsigned int) taken << 1) | (1 - taken)) << TAG_LEN) | ((bi.address) & ((1<<TAG_LEN) - 1));
        } else {
          // This is the case that missed in the last table. We just update the
          // count like a normal two-bit predictor
          update_prediction(prov_pred, taken);
        }
			}
		}
    // Moved this function out for clarity
    advance_history(taken);
	}
};
