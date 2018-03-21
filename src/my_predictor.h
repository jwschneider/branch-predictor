// tagged-hybrid_predictor.h
// The tagged hybrid predictor uses cascading prediction tables
// indexed by a hash of the history and the pc.
// Current hash implementation is a simple xor hash.
#include <vector>
#include <stdlib.h>
#include <iostream>
#include <stdio.h>
#include <math.h>
using std::vector;
using std::cout;
using std::endl;

class my_update : public branch_update {
public:
  // Instead of keeping the table and index, we keep the
  // provider, prov, and the alternate, alt
  unsigned int prov;
  unsigned int alt;
};

class my_predictor : public branch_predictor {
public:
const unsigned int TABLE_BITS	= 12;       // Number of rows in the table
const unsigned int HISTORY_LEN = 64;      // Length of history recorded
const unsigned int TAG_LEN = 6;           // Length of tag entries
const unsigned int TABLE_CT = 8;          // Number of tables
const bool UPDATE_ALT_ON_HIT = true;      // Whether we update the alternate on a providor hit
	my_update u;
	branch_info bi;
	unsigned long long hist;                // Since history is capped at 64, we can fit it into a long 
	vector<unsigned int *> pred; 


  // Initialize the tables to weakly not taken
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

  // Hashes together the PC and some amount of the history. We use xor as
  // the hash function. In the case that the amount of history we want to hash
  // is larger than the bits table length, we fold the history down and hash
  // all the bits into the space of the table length
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
      unsigned int &prov_pred = *(pred[y->prov] + table_index(bi.address, y->prov));
      unsigned int &alt_pred = *(pred[y->alt] + table_index(bi.address, y->alt));
			if (taken == y->direction_prediction()) {
        // On a correct prediction, bump the prediction counters
        update_prediction(prov_pred, taken);
        if (UPDATE_ALT_ON_HIT)
          update_prediction(alt_pred, taken);
			} else {
        // On an incorrect prediction, we must allocate space in a new table
        if (y->prov < TABLE_CT - 1) {
          srand(1);
          // Use table i+1 50% of the time, i+2 and i+3 each 25% of the time
          int rNum = rand()%2; 
          unsigned int tbl_ct = y->prov; 
          if (rNum) {
            tbl_ct = (y->prov + 1);
          } else {
            rNum = rand()%2;
            if (rNum) {
              tbl_ct = (y->prov + 2);
            } else {
              tbl_ct = (y->prov + 3);
            }
          }
          if (tbl_ct >= TABLE_CT) {
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
    advance_history(taken);
	}
};
