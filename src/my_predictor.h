// twobit_predictor.h
// This file is the sample predictor shipped with the package
// simplified to be just a two bit predictor indexed by the PC
// Added comments for clarity
//
class my_update : public branch_update {
public:
	unsigned int index;
};

class my_predictor : public branch_predictor {
public:
#define TABLE_BITS	10      // table has 2^10 rows
	my_update u;
	branch_info bi;
	unsigned char tab[1<<TABLE_BITS];   // each table entry is a char
                                        // 0 is strongly not taken, 3 is
                                        //   strongly taken

	my_predictor (void) { 
		memset (tab, 0, sizeof (tab));  // initialize the table to all strongly
                                        // not taken
	}

	branch_update *predict (branch_info & b) {
		bi = b;
		if (b.br_flags & BR_CONDITIONAL) {
			u.index = (b.address & ((1<<TABLE_BITS)-1));    // index the table by PC
			u.direction_prediction (tab[u.index] >> 1);
		} else {
			u.direction_prediction (true);                  // take non-conditional branches
		}
		u.target_prediction (0);                          // not doing any target prediction
		return &u;
	}

	void update (branch_update *u, bool taken, unsigned int target) {
		if (bi.br_flags & BR_CONDITIONAL) {               
			unsigned char *c = &tab[((my_update*)u)->index];
			if (taken) {                      // i dont know why they bother with char * here,
                                        // but theyre the c++ expert, not me
				if (*c < 3) (*c)++;             // update the table with prediction result
			} else {
				if (*c > 0) (*c)--;
			}
		}
	}
};
