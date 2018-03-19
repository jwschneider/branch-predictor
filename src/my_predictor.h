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

class my_update : public branch_update {
public:
  unsigned int index;     // index in the table that was updated
  unsigned int table;     // which table was updated, 0 being the base
};

class my_predictor : public branch_predictor {
public:
const unsigned int TABLE_BITS	= 12;   // start with 2^10 rows
const unsigned int HISTORY_LEN = 32;    // start with 4 bit history length, as in the
                                        // book example  
const unsigned int TAG_LEN = 6;         // so that the prediction ang tag can fit into an
                                        // unsigned int, we use 30 bit tags
const unsigned int TABLE_CT = 6;

	my_update u;
	branch_info bi;
  unsigned char hist;

	vector<unsigned int *> pred; 

	my_predictor (void) : hist(0) { 
		/* iterate through all history and set to 0 for all entries */
		for (unsigned int i = 0; i < TABLE_CT; i++) {
		unsigned int* tbl = (unsigned int*) malloc((1 << TABLE_BITS) * sizeof(unsigned int));
    // what to initialize predictions to
    for (int i = 0; i < (1<<TABLE_BITS); i++) {
      *(tbl + i) = 1 << (TAG_LEN + 1);
    }
		pred.push_back(tbl);
    } 
	}

  unsigned int address_hash(unsigned int address, unsigned int history) {
    return address ^ history;
  }

  // returns the i'th element of the geometric progression,
  // or 0 if i is 0.
  unsigned int gp(int base, int i) {
    if (i == 0)
      return 0;
    else
      return pow(base, i);
  }

  // returns the bottom i bits of hist
  unsigned int hist_mask(int i) {
    return hist & ((1<<i)-1);
  }

	branch_update *predict (branch_info &b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
	    	u.table = 0;
	    	u.index = b.address & ((1 << TABLE_BITS) - 1);
			for (unsigned int tableCounter = 1; tableCounter < TABLE_CT; tableCounter++) {
	        	if ((*(pred[tableCounter] + (address_hash(b.address, hist_mask(gp(2, tableCounter))) & ((1<<TABLE_BITS) - 1))) 
                 & ((1<<TAG_LEN) - 1)) 
                 ==	(b.address & ((1<<TAG_LEN) - 1))) {
					// if the item in the table matches the current item, update it
              u.table = tableCounter;
					    u.index = (address_hash(b.address, hist_mask(gp(2, tableCounter))) & ((1<<TABLE_BITS) - 1));
				}
				// increment counter
			}
	      	unsigned int *tbl = pred[u.table]; 
	      	u.direction_prediction (*(tbl + u.index) >> (TAG_LEN + 1));
		} else {
			u.direction_prediction (true);                  
		}
		u.target_prediction (0);                          
		//cout << u.table << endl << u.index << endl;
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {               
			my_update* y = (my_update*) u;
			unsigned int* tbl = pred[y->table];
			unsigned int prediction = *(tbl + y->index) >> (TAG_LEN);
      //if (y->index == 963) 
      //  printf("Address: %d\tIndex: %d\tTable: %d\tPrediction: %d\t Taken: %d\n",
      //     bi.address, y->index, y->table, prediction, taken);
			//printf ("%d\t%d\t%d\t%d\n", y->table, y->index, prediction, taken);
			if (taken == (prediction >> 1)) {
			// if prediction was correct
				if (taken) {
					if (prediction < 3) prediction++;             
				} else {
					if (prediction > 0) prediction--;
				}
				*(tbl + y->index) = (*(tbl + y->index) &
          			((1<<TAG_LEN) - 1)) | (prediction << TAG_LEN);
          		
          		if ((y->table) > 0) {
          			// if update table has previous, update there too
					unsigned int* new_tbl = pred[(y->table) - 1];
					if (y->table - 1 == 0) {
						*(new_tbl + y->index) = (prediction << TAG_LEN);
					} else {
          				*(new_tbl + y->index) = (*(tbl + y->index) &
          					((1<<TAG_LEN) - 1)) | (prediction << TAG_LEN);
          			}
          		}
			} else {
			// if prediction was incorrect, allocate space 
        //printf ("Mispredict!\t Table:%d\t Index:%d\t Prediction:%d\t Taken:%d\t History:%d\n", y->table, y->index, prediction, taken, hist);
				srand(1);
				int rNum = rand()%2;
				unsigned int t_index = y->table;
				if (rNum) {
					// rNum is odd, update table i + 1
					t_index = (y->table + 1);
				} else {
					// if rNum is even, update either i+2, i+3
					rNum = rand()%2;
					if (rNum) {
						// if rNUm is odd, update table i+2
						t_index = (y->table + 2);
					} else {
						// if rNum is even, update table i+3
						t_index = (y->table + 3);
					}
				}
				if (t_index >= TABLE_CT) {
					// if our adjustment has caused us to go out-of-bounds
					if (y->table + 1 < TABLE_CT) {
						// if we have a one-right table to update, update that
						t_index = y->table+1;
					} else {
						// otherwise, just update the last table
						t_index = TABLE_CT - 1;
					}
				}
				*(pred[t_index] + (address_hash(bi.address, hist_mask(gp(2, t_index))) & ((1<<TABLE_BITS) - 1))) =
					 ((((unsigned int) taken << 1) | (1 - taken)) << TAG_LEN) | ((bi.address) & ((1<<TAG_LEN) - 1));
				// store the updated prediction in table
			}
		  	hist = ((hist << 1) | taken) & ((1<< HISTORY_LEN) - 1); 
      
		}
	}
};
