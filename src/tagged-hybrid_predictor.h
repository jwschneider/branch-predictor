// tagged-hybrid_predictor.h
// The tagged hybrid predictor uses cascading prediction tables
// indexed by a hash of the history and the pc.
// Current hash implementation is a simple xor hash; TBU.
#include <vector>
using std::vector;

class my_update : public branch_update {
public:
  unsigned int index;     // index in the table that was updated
  unsigned int table;     // which table was updated, 0 being the base
};

class my_predictor : public branch_predictor {
public:
const unsigned int TABLE_BITS	= 10;   // start with 2^10 rows
const unsigned int HISTORY_LEN = 4;    // start with 4 bit history length, as in the
                                        // book example  
const unsigned int TAG_LEN = 30;       // so that the prediction ang tag can fit into an
                                        // unsigned int, we use 30 bit tags
	my_update u;
	branch_info bi;
  unsigned char hist;

	vector<unsigned int *> pred; 

	my_predictor (void) : hist(0) { 
		/* iterate through all history and set to 0 for all entries */
		for (unsigned int i = 0; i < HISTORY_LEN + 1; i++) {
    	unsigned int tbl[1<<TABLE_BITS];
    	memset (tbl, 0, sizeof (tbl));
      pred.push_back(tbl);
     	} 
	}

	branch_update *predict (branch_info &b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			/* TODO: did you want this to be logical and or bitwise and? */
	    unsigned int ct = 0;
      u.table = 0;
      u.index = b.address & ((1 << TAG_LEN) - 1);
			for (vector<unsigned int *>::iterator it = pred.begin(); it != pred.end(); it++) {
				// for each item in the history list
        if (*(*it + ((b.address ^ (hist & ct)) & ((1<<TAG_LEN) - 1))) ==
            (b.address & ((1<<TAG_LEN) - 1))) {
					// if the item in the table matches the current item, update it
					u.table = ct;
					u.index = ((b.address ^ (hist & ct)) & ((1<<TAG_LEN) - 1));
				}
				// increment counter
				ct++;
			}
      unsigned int *tbl = pred[u.table];
			u.direction_prediction (*(tbl + u.index) >> (TAG_LEN + 1));
		} else {
			u.direction_prediction (true);                  
		}
		u.target_prediction (0);                          
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {               
    // TODO: implement the update functionality
      hist = ((hist << 1) | taken) & ((1<< HISTORY_LEN) - 1); 
      unsigned int *tbl = pred[((my_update*)u)->table];
      unsigned int prediction = *(tbl + ((my_update*)u)->index) >> TAG_LEN;
			if (taken) {                      
				if (prediction < 3) prediction++;             
			} else {
				if (prediction > 0) prediction--;
			}
      *(tbl + ((my_update*)u)->index) = (*(tbl + ((my_update*)u)->table) &
          ((1<<TAG_LEN) - 1)) |
          (prediction << TAG_LEN);
		}
	}
};
