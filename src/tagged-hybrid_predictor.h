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
#define TAG_LEN 30        // so that the prediction ang tag can fit into an
                          // unsigned int, we use 30 bit tags
	my_update u;
	branch_info bi;
    unsigned char hist;

	std::list<unsigned int *> pred (HISTORY_LEN + 1); 

	my_predictor (void) : hist(0) { 
		/* iterate through all history and set to 0 for all entries */
		for (std::list<unsigned int *>::iterator it = pred.begin(); it != pred.end(); it++) {
    	*it = new unsigned int[1<<TABLE_BITS];
    	memset (*it, 0, 1<<TABLE_BITS);
     	} 
	}

	branch_update *predict (branch_info &b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			/* TODO: did you want this to be logical and or bitwise and? */
	    	unsigned int ct = 0;
			for (std::list<unsigned int *>::iterator it = pred.begin(); it != pred.end(); it++) {
				// for each item in the history list
				if ((*(*it + (b.address ^ (hist & ct))) & (1<<TAG_LEN - 1)) ==
					(b.address & (1 <<TAG_LEN - 1))) {
					// if the item in the table matches the current item, update it
					u.table = ct;
					u.index = (b.address ^ (hist & ct));
					u.direction_prediction (*(*it + u.index) >> TAG_LEN);
					break;
				} else if (it == pred.end()) {
					// otherwise, if this is the last item
					u.table = 0;
					u.index = b.address;
					u.direction_prediction (*(*l.begin() + u.index >> TAG_LEN));
					// TODO: should this be an expression ^ ?
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
		if (bi.br_flags & BR_CONDITIONAL) {               
    // TODO: implement the update functionality
			if (taken) {                      
				if (*c < 3) (*c)++;             
			} else {
				if (*c > 0) (*c)--;
			}
		}
	}
};
