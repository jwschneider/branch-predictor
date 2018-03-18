// tagged-hybrid_predictor.h
// The tagged hybrid predictor uses cascading prediction tables
// indexed by a hash of the history and the pc.
// Current hash implementation is a simple xor hash; TBU.
#include <list>

class my_update : public branch_update {
public:
  unsigned int index;     // index in the table that was updated
  unsigned int table;     // which table was updated, 0 being the base
};

class my_predictor : public branch_predictor {
public:
#define TABLE_BITS	10    // start with 2^10 rows
#define HISTORY_LEN 4     // start with 4 bit history length, as in the
                          // book example  
#define TAG_LEN 30        // so that the prediction and tag can fit into an
                          // unsigned int, we use 30 bit tags
	my_update u;
	branch_info bi;
    unsigned char hist;

	std::list<unsigned int *> pred (HISTORY_LEN + 1); 

	my_predictor (void) : hist(0) { 
		/* iterate through all history and set to array of 0's 
			(size 1<<table_bits) for all entries */
		for (std::list<unsigned int *>::iterator it = pred.begin(); it != pred.end(); it++) {
	    	*it = new unsigned int[1<<TABLE_BITS];
	    	memset (*it, 0, 1<<TABLE_BITS);
     	} 
	}

	branch_update *predict (branch_info &b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			/* if branch is a conditional branch, we can predict on it */
	    	unsigned int ct = 0;
			for (std::list<unsigned int *>::iterator it = pred.begin(); it != pred.end(); it++) {
				// for each table in the history list
				if ((*(*it + (b.address ^ (hist & ct))) & (1<<TAG_LEN - 1)) ==
					(b.address & (1 <<TAG_LEN - 1))) {
					// if the entry at hash is equal to the hash
					u.table = ct;
					u.index = (b.address ^ (hist & ct));
					u.direction_prediction (*(*it + u.index) >> TAG_LEN);
					break;
				} else if (it == pred.end()) {
					// otherwise, if we haven't found where it goes, put it at the end (?)
					u.table = 0;
					u.index = b.address;
					u.direction_prediction (*(*l.begin() + u.index >> TAG_LEN));
				}
				// increment counter
				ct++;
			}
		} else {
			u.direction_prediction (true);                  
		}
		u.target_prediction (0);                          
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
	// TODO: implement the update functionality
		if (bi.br_flags & BR_CONDITIONAL) {               
    	// if the branch is a conditional branch (?)
			if (taken) {
			// index into the table, increase the counter for the same tagged entry                    
				if (*c < 3) (*c)++;             
			} else {
			// index into the table, decrease the counter for the same tagged entry
				if (*c > 0) (*c)--;
			}
		}
	}



};
