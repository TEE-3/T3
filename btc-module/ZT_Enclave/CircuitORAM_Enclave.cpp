/*
*    ZeroTrace: Oblivious Memory Primitives from Intel SGX 
*    Copyright (C) 2018  Sajin (sshsshy)
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, version 3 of the License.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#include "CircuitORAM_Enclave.hpp"

//TODO: Move oarray_search to globals from PathORAM and CircuitORAM
void oarray_search2(uint32_t *array, uint32_t loc, uint32_t *leaf, uint32_t newLabel,uint32_t N_level) {
    for(uint32_t i=0;i<N_level;i++) {
        omove(i,&(array[i]),loc,leaf,newLabel);
    }
    return;
}

void CircuitORAM::Initialize(uint8_t pZ, uint32_t pmax_blocks, uint32_t pdata_size, uint32_t pstash_size, uint32_t poblivious_flag, uint32_t precursion_data_size, int8_t precursion_levels, uint64_t onchip_posmap_mem_limit){
	#ifdef BUILDTREE_DEBUG
		printf("In CircuitORAM::Initialize, Started Initialize\n");
	#endif

	ORAMTree::SampleKey();	
	ORAMTree::SetParams(pZ, pmax_blocks, pdata_size, pstash_size, poblivious_flag, precursion_data_size, precursion_levels, onchip_posmap_mem_limit);
	ORAMTree::Initialize();

	uint32_t d_largest;
	if(recursion_levels==-1)
		d_largest = D;
	else
		d_largest = D_level[recursion_levels];

	deepest = (uint32_t*) malloc (sizeof(uint32_t) * (d_largest+2));
	target = (uint32_t*) malloc (sizeof(uint32_t) * (d_largest+2));
	deepest_position = (int32_t*) malloc ((d_largest+2) * sizeof(uint32_t) );
	target_position = (int32_t*) malloc ((d_largest+2) * sizeof(uint32_t) );
	serialized_block_hold = (unsigned char*) malloc (data_size + ADDITIONAL_METADATA_SIZE);
	serialized_block_write = (unsigned char*) malloc (data_size + ADDITIONAL_METADATA_SIZE);	

	#ifdef BUILDTREE_DEBUG
		printf("Finished Initialize\n");
	#endif
}

uint32_t* CircuitORAM::prepare_target(uint32_t N, uint32_t D, uint32_t leaf, unsigned char *serialized_path, uint32_t block_size, uint32_t level, uint32_t * deepest, int32_t *target_position){	
			int32_t i,k;
			nodev2* listptr_t;
			int32_t src = -1, dest = -1;
			unsigned char *serialized_path_ptr = serialized_path;

			if(recursion_levels!=-1)
				listptr_t = recursive_stash[level].getStart();		
			else
				listptr_t = stash.getStart();
		

			for(i = D+1; i >= 0 ; i--) {
				target[i] = -1;
				
				// If i==src : target[i] = dest, dest = -1 , src = -1
				pt_settarget(&(target[i]), &dest, &src, i==src);
			
				//Fix for i = 0
				if(i==0) {

				}
				else{
					uint32_t flag_empty_slot = 0;
					uint32_t target_position_set = 0;
					for(k=0;k<Z;k++) {
						bool dummy_flag = isBlockDummy(serialized_path_ptr, gN);
						flag_empty_slot = (flag_empty_slot || dummy_flag);
					
						uint32_t flag_stp = dummy_flag && (!target_position_set);

						//if flag_stp: target_position[i] = k, target_set = 1 
						pt_set_target_position(&(target_position[i]), k, &target_position_set, flag_stp);
						//Oblivious Function for below snippet
						/*
						if(block.isDummy()&&(!target_set)) {
							target_position[i] = k;
							target_set = 1;		
						}
						*/
						serialized_path_ptr+=(block_size);		
					}

					uint32_t flag_pt = ( ((dest==-1 && flag_empty_slot) ) && (deepest[i]!= -1) );
					//if flag_pt : src = deepest[i], dest = i
					pt_set_src_dest(&src, &dest, deepest[i], i, flag_pt);
					//Rewrite as oblivious Function
					/*
					if(flag_pt) {
						src = deepest[i-1];
						dest = i;
					}
					*/
				}			
			}	
			
			return target;		
		}

// Root - to - leaf linear scan , NOTE: serialized_path from LS is filled in leaf to root
// Creates deepest[] , where deepest[i] stores the source level of the deepest block in path[0,...,i-1] that can reside in path[i]
uint32_t* CircuitORAM::prepare_deepest(uint32_t N, uint32_t D, uint32_t leaf, unsigned char *serialized_path, uint32_t block_size, uint32_t level, int32_t *deepest_position){
    uint32_t i,k;
    unsigned char *serialized_path_ptr = serialized_path + (Z*(D+1)-1)*(block_size);			
    uint32_t local_deepest = 0;
    int32_t goal = -1, src = -1;
    nodev2* listptr_t;

    if(recursion_levels!=-1)
        listptr_t = recursive_stash[level].getStart();		
    else
        listptr_t = stash.getStart();

    for(i = 0 ; i <= D+1;i++) {

        if(i==0) {
            //deepest[stash]
            for(k=0;k<stash_size;k++) {
                uint32_t l1 = getTreeLabel(listptr_t->serialized_block) + N;
                uint32_t l2 = leaf + N;

                #ifdef ACCESS_CORAM_DEBUG												
                    printf("(%d, %d) - local_deepest = %d\n",getTreeLabel(listptr_t->serialized_block) + N, leaf + N, local_deepest);
                #endif

                //HERE D+1 -> D ?
                uint32_t iter_position_in_path = D+1, position_in_path = 0;
                for(uint32_t j = 0;j < D+1; j++) {
                    uint32_t flag_deepest = (l1==l2) && (!isBlockDummy(listptr_t->serialized_block, gN) && l1>local_deepest);
                    oset_value(&local_deepest, l1, flag_deepest);
                    oset_value((uint32_t*) &(deepest_position[i]), k, flag_deepest);		
                    oset_value(&position_in_path, iter_position_in_path, flag_deepest);

                    #ifdef ACCESS_CORAM_DEBUG3
                        if(!isBlockDummy(listptr_t->serialized_block, gN)){
                            printf("\t(%d, %d) - local_deepest = %d, iter_position_in_path = %d\n",
                                l1, l2, local_deepest, iter_position_in_path);
                        }
                    #endif

                    l1=l1>>1;
                    l2=l2>>1;
                    iter_position_in_path-=1;
                }

                oset_value((uint32_t*) &(deepest_position[i]), k, position_in_path > goal);
                oset_goal_source(i, position_in_path, (int32_t) position_in_path > goal && position_in_path > i, &src, &goal);

                listptr_t = listptr_t->next;
            }

        }
        else{

            //if goal > i, deepest[i] = goal
            deepest[i] = -1;		
            uint32_t flag_pdsd = (goal >= (int32_t) i) ;
            pd_setdeepest(&(deepest[i]), src, flag_pdsd);
            #ifdef ACCESS_CORAM_DEBUG3
                printf("i = %d, Goal is %d , src = %d, deepest[i] = %d\n",i,goal, src, deepest[i]);
            #endif

            oset_value((uint32_t*) &src, (uint32_t) -1, goal == i);
            oset_value((uint32_t*) &goal, (uint32_t) -1, goal == i);



                for(k=0;k<Z;k++) {
                bool dummy_flag = isBlockDummy(serialized_path_ptr, gN);
                uint32_t l1 = getTreeLabel(serialized_path_ptr) + N;
                uint32_t l2 = leaf + N;
                uint32_t iter_position_in_path = D+1, position_in_path = 0;

                #ifdef ACCESS_CORAM_DEBUG												
                    printf("(%d, %d) - local_deepest = %d\n",getTreeLabel(serialized_path_ptr) + N, leaf + N, local_deepest);
                #endif

                for(uint32_t j = 0;j < D + 1 - i; j++) {
                    uint32_t flag_deepest = (l1==l2) && !dummy_flag && l1>local_deepest ;
                    oset_value(&local_deepest, l1, flag_deepest);
                    oset_value((uint32_t*) &(deepest_position[i]), k, flag_deepest);
                    oset_value(&position_in_path, iter_position_in_path, flag_deepest);

                    #ifdef ACCESS_CORAM_DEBUG3
                        if(!dummy_flag)								
                        printf("\t(%d, %d) - local_deepest = %d, deepest_position[%d] = %d\n",l1, l2, local_deepest,i-1, deepest_position[i-1]);
                    #endif
                    l1=l1>>1;
                    l2=l2>>1;
                    iter_position_in_path-=1;
                }
                oset_value((uint32_t*) &(deepest_position[i]), k, position_in_path > goal);
                oset_goal_source(i, position_in_path, (int32_t) position_in_path > goal && position_in_path > i, &src, &goal);
                serialized_path_ptr-=(block_size);

            }


        }
    }
    //deepest[0] = _|_
    deepest[0] = -1;
    return deepest;
}

//Pass from Root to leaf (Opposite direction of serialized_path)
void CircuitORAM::EvictOnceFast(uint32_t *deepest, uint32_t *target, int32_t* deepest_position, int32_t* target_position , unsigned char * serialized_path, unsigned char* path_hash, uint32_t dlevel, uint32_t nlevel, uint32_t level, unsigned char *new_path_hash, uint32_t leaf){
	int32_t hold = -1, dest = -1;
	uint32_t write_flag = 0;
	uint32_t tblock_size, tdata_size;

	//Check level and set block size
	if(recursion_levels!=-1) {
		if(level==recursion_levels) {
			tblock_size = data_size + ADDITIONAL_METADATA_SIZE;
			tdata_size = data_size;	
		} 
		else {
			tblock_size = recursion_data_size + ADDITIONAL_METADATA_SIZE;				
			tdata_size = recursion_data_size;			
		}
	} 
	else {
		tblock_size = data_size + ADDITIONAL_METADATA_SIZE;
		tdata_size = data_size;			
	}

	unsigned char *serialized_path_ptr = serialized_path + ((Z*(dlevel+1))-1)*(tblock_size);
	unsigned char *serialized_path_ptr2 = serialized_path_ptr;

	nodev2 *listptr_t;
	if(recursion_levels!=-1)
		listptr_t = recursive_stash[level].getStart();		
	else
		listptr_t = stash.getStart();
	write_flag = 0;

	for(uint32_t i = 0;i<=(dlevel+1);i++) {
		// Oblivious function for Setting Write flag, hold , dest and moving held_block to block_write				
		uint32_t flag_1 = ( (hold!=-1) && (i==dest) );
		omove_serialized_block(serialized_block_write, serialized_block_hold, tdata_size, flag_1);
		//oset_hold_dest : Set hold = -1, dest = -1, write_flag =1
		oset_hold_dest(&hold, &dest, &write_flag, flag_1);

		if(i==0) {
			//Scan the blocks in Stash to pick deepest block from it
			for(uint32_t k = 0; k< stash_size; k++) {
				unsigned char *stash_block = listptr_t->serialized_block;
				uint32_t flag_hold = (k == deepest_position[i] && (target[i]!=-1) && (!isBlockDummy(stash_block, gN)) ); 	

				#ifdef DEBUG_EFO
					if(flag_hold)
						printf("Held block ID = %d, treelabel = %d\n",getId(stash_block), getTreeLabel(stash_block));				
				#endif

				omove_serialized_block(serialized_block_hold, stash_block, tdata_size, flag_hold);
				oset_value(getIdPtr(stash_block), gN, flag_hold);
				oset_value((uint32_t*) &dest, target[i], flag_hold);
				oset_value((uint32_t*) &hold, target[i], flag_hold);
				listptr_t = listptr_t->next;
			}
	
		}
		else{
			//Scan the blocks in Path[i] to pick deepest block from it
			for(uint32_t k = 0;k < Z; k++){
				uint32_t flag_hold = (k == deepest_position[i])&&(hold==-1)&&(target[i]!=-1); 					
				omove_serialized_block(serialized_block_hold, serialized_path_ptr, tdata_size, flag_hold);
				#ifdef DEBUG_EFO
					if(flag_hold)							
						printf("Held Block in bucket, ID = %d, treelabel = %d\n", getId(serialized_block_hold),getTreeLabel(serialized_block_hold));
				#endif
				uint32_t *temp_pos = (uint32_t*) serialized_path_ptr;
				oset_value((uint32_t*) &(temp_pos[4]), gN, flag_hold);
				oset_value((uint32_t*) &dest, target[i], flag_hold);	
				oset_value((uint32_t*) &hold, target[i], flag_hold);							
				serialized_path_ptr -= tblock_size;
			}
		}

		//if(target[i]!= -1)
		//	dest = target[i];
		uint32_t flag_t = ((int32_t)target[i]!= -1);
		oset_value((uint32_t *) &dest, target[i], flag_t);

		if(i!=0) {
			uint32_t flag_w =0; 
			for(int32_t k = (Z-1);k >= 0; k--){
				flag_w = ( write_flag && (k == target_position[i]) );
				#ifdef DEBUG_EFO
					if(flag_w)
						printf("serialized_path_ptr2 : ID = %d, treeLabel = %d\n",getId(serialized_path_ptr2),
					getTreeLabel(serialized_path_ptr2));	
				#endif						

				omove_serialized_block(serialized_path_ptr2, serialized_block_write, tdata_size, flag_w);
	
				#ifdef DEBUG_EFO
					printf("i = %d, k = %d, write_flag = %d, flag_w = %d, writeblock_id = %d, writeblock_treelabel = %d\n", i, k, write_flag, flag_w,
						getId(serialized_block_write), getTreeLabel(serialized_block_write));
					if(flag_w)
						printf("serialized_path_ptr2 : ID = %d, treeLabel = %d\n",getId(serialized_path_ptr2),
					getTreeLabel(serialized_path_ptr2));			
				#endif 					

				oset_value(&write_flag, (uint32_t) 0, flag_w);
				serialized_path_ptr2-=tblock_size;
			}
		}
    	}					
}	

void CircuitORAM::CircuitORAM_FetchBlock(unsigned char* decrypted_path, unsigned char *encrypted_path, unsigned char* serialized_result_block, unsigned char *path_hash, 
        unsigned char *new_path_hash, uint32_t path_size, uint32_t new_path_hash_size, uint32_t *return_value, uint32_t leaf, uint32_t newleaf, char opType,
        uint32_t id, uint32_t position_in_id, uint32_t newleaf_nextlevel, uint32_t data_size, uint32_t block_size, uint32_t level, uint32_t dlevel, 
        uint32_t nlevel, unsigned char *data_in, unsigned char *data_out) {

	//FetchBlock over Path
	unsigned char *decrypted_path_ptr = decrypted_path;
	unsigned char *path_ptr;
	unsigned char *new_path_hash_iter = new_path_hash;
	unsigned char *new_path_hash_trail = new_path_hash;
	unsigned char *old_path_hash_iter = path_hash;		
	unsigned char *new_path_hash_ptr = new_path_hash;
	uint32_t leaf_temp_prev = (leaf+nlevel)<<1;
	uint32_t i,k; 
	uint8_t rt;

	#ifdef ACCESS_DEBUG
		printf("Fetched Path : \n");
		showPath_reverse(decrypted_path, Z*(dlevel+1), data_size);
	#endif

	for(i=0;i < ( Z * (dlevel+1) ); i++) {
		if(oblivious_flag) {
			uint32_t block_id = getId(decrypted_path_ptr);
			//if(block_id==id&&level==recursion_levels)
			//	printf("Flag for move to serialized_result_block SET ! \n");
			omove_serialized_block(serialized_result_block, decrypted_path_ptr, data_size, block_id==id);
			oset_block_as_dummy(getIdPtr(decrypted_path_ptr), gN, block_id==id);
		}
		else{
			uint32_t block_id = getId(decrypted_path_ptr);
			if(block_id==id) {
				memcpy(serialized_result_block, decrypted_path_ptr, block_size);
				setId(decrypted_path_ptr, gN);
			}
		}					
		decrypted_path_ptr+=(block_size);
	}

	//FetchBlock over Stash
	nodev2 *listptr_t;
	if(recursion_levels!=-1)
		listptr_t = recursive_stash[level].getStart();		
	else
		listptr_t = stash.getStart();

	if(oblivious_flag) {
		for(k=0; k < stash_size; k++) {
			unsigned char* stash_block = listptr_t->serialized_block;
			uint32_t block_id = getId(stash_block);
			bool flag_move = (block_id == id && !isBlockDummy(stash_block, gN));
			omove_serialized_block(serialized_result_block, stash_block, data_size, flag_move);
			listptr_t=listptr_t->next;
		}
		if(level!= recursion_levels){
			uint32_t *data_iter = (uint32_t*) getDataPtr(serialized_result_block);
			for(k=0; k<x; k++){
				uint32_t flag_ore = (position_in_id==k); 										
				oset_value(return_value, data_iter[k], flag_ore);
				oset_value(&(data_iter[k]), newleaf_nextlevel, flag_ore);
			}
		}
	}

	else{
		for(k=0; k < stash_size; k++)
		{
			unsigned char* stash_block = listptr_t->serialized_block;
			if(getId(stash_block)==id){
				memcpy(serialized_result_block, stash_block, block_size);
				setId(stash_block, gN);
			}
			listptr_t=listptr_t->next;
		}

		if(level!= recursion_levels){
			uint32_t *data_iter = (uint32_t*) getDataPtr(serialized_result_block);
			data_iter[position_in_id]=newleaf_nextlevel;
		} 
	}

	#ifdef ACCESS_DEBUG
		displaySerializedBlock(serialized_result_block, level, recursion_levels, x);
	#endif

	//Encrypt Path Module
	#ifdef ENCRYPTION_ON
		encryptPath(decrypted_path, encrypted_path, (Z*(dlevel+1)), data_size);
	#endif			

	//Path Integrity Module
	#ifndef PASSIVE_ADVERSARY
		new_path_hash_iter = new_path_hash;
		new_path_hash_trail = new_path_hash;
		old_path_hash_iter = path_hash;		
		new_path_hash_ptr = new_path_hash;
		leaf_temp_prev = (leaf+nlevel)<<1;

		#ifdef ENCRYPTION_ON
			path_ptr = encrypted_path;
		#else
			path_ptr = decrypted_path;
		#endif

		for(i=0;i < ( Z * (dlevel+1) ); i++) {
			if(i%Z==0) {
				uint32_t p = i/Z;
				addToNewPathHash(path_ptr, old_path_hash_iter, new_path_hash_trail, new_path_hash_iter,(dlevel+1)-p, leaf_temp_prev, block_size, dlevel, level);
				leaf_temp_prev>>1;
				path_ptr+=(Z*block_size);
			}
		}
	#endif

	// Time taken signal !
	if(level==recursion_levels){
		#ifdef TIME_PERFORMANCE
			time_report(2);
		#endif
	}	

	// WriteBack the path, arr_blocks
	#ifdef ENCRYPTION_ON
		uploadPath(&rt, encrypted_path, path_size, leaf + nlevel, new_path_hash, new_path_hash_size, level, dlevel);			
	#else
		uploadPath(&rt, decrypted_path, path_size, leaf + nlevel, new_path_hash, new_path_hash_size, level, dlevel);
	#endif	

	//Set newleaf for fetched_block
	setTreeLabel(serialized_result_block, newleaf);

	//Insert Fetched Block into Stash
	if(oblivious_flag){
		if(recursion_levels!=-1) {
			recursive_stash[level].pass_insert(serialized_result_block, isBlockDummy(serialized_result_block,gN));
		}				
		else{
			stash.pass_insert(serialized_result_block, isBlockDummy(serialized_result_block, gN));
		}			
	}
	else{
		if(recursion_levels!=-1) 
			recursive_stash[level].insert(serialized_result_block);
		else
			stash.insert(serialized_result_block);
	}

	if(level == recursion_levels){
		recursive_stash[level].PerformAccessOperation(opType, id, newleaf, data_in, data_out);
	}

}


uint32_t CircuitORAM::access_oram_level(char opType, uint32_t leaf, uint32_t id, uint32_t position_in_id, uint32_t level, uint32_t newleaf,uint32_t newleaf_nextleaf, unsigned char *data_in,  unsigned char *data_out)
{
	uint32_t return_value=-1;
	#ifdef EXITLESS_MODE			
		path_hash = resp_struct->path_hash;
	#endif

	decrypted_path = ReadBucketsFromPath(leaf + N_level[level], path_hash, level);

  // ADIL.
  if (opType == 'r') {
    printf("[ADIL]. [WARNING, Not properly checked!] Reading from Level: %d\n", level);
	  return_value = CircuitORAM_Access_Read(opType, id, position_in_id,leaf, newleaf, newleaf_nextleaf,decrypted_path, 
					path_hash,level,D_level[level],N_level[level], data_in, data_out); 
  } else {
	  return_value = CircuitORAM_Access(opType, id, position_in_id,leaf, newleaf, newleaf_nextleaf,decrypted_path, 
					path_hash,level,D_level[level],N_level[level], data_in, data_out); 
  }
    return return_value;		
}

void CircuitORAM::EvictionRoutine(unsigned char *decrypted_path, unsigned char *encrypted_path, unsigned char *path_hash, unsigned char *new_path_hash, 
				uint32_t data_size, uint32_t block_size, uint32_t path_size, uint32_t new_path_hash_size, uint32_t leaf, uint32_t level, 
				uint32_t dlevel, uint32_t nlevel) {
	uint8_t rt;			
	uint32_t i;
	uint64_t leaf_right, leaf_left, temp_n;
	unsigned char *eviction_path_left, *eviction_path_right;
	unsigned char *decrypted_path_ptr = decrypted_path;
	unsigned char *path_ptr;
	unsigned char *new_path_hash_iter = new_path_hash;
	unsigned char *new_path_hash_trail = new_path_hash;
	unsigned char *old_path_hash_iter = path_hash;		
	unsigned char *new_path_hash_ptr = new_path_hash;
	uint32_t leaf_temp_prev = (leaf+nlevel)<<1;
	
	#ifdef SHOW_STASH_COUNT_DEBUG
		print_stash_count(level, nlevel);	
	#endif
		
	//Sample the leaves for eviction
	sgx_read_rand((unsigned char*) &temp_n, 4);
	leaf_left = (temp_n % (nlevel/2) );
	leaf_right = leaf_left + (nlevel/2);

	eviction_path_left = ReadBucketsFromPath(leaf_left + nlevel, path_hash, level);
	for(uint32_t e = 0; e <dlevel+2; e++){
		deepest_position[e] = -1;
		target_position[e] = -1;
	}
            
	#ifdef SHOW_STASH_CONTENTS
		if(level==recursion_levels)
			recursive_stash[level].displayStashContents(nlevel);
	#endif

	#ifdef ACCESS_DEBUG			
		printf("Level = %d, leaf_left = %d, with + nlevel = %d, Eviction_path_left:\n",level, leaf_left, leaf_left + nlevel);
		showPath_reverse(eviction_path_left, Z*(dlevel+1), data_size);
		print_stash_count(level,nlevel);
	#endif

	// prepare_deepest() Create deepest[]
	deepest = prepare_deepest(nlevel, dlevel, leaf_left, eviction_path_left, block_size, level, deepest_position);

	#ifdef ACCESS_CORAM_META_DEBUG
		printf("Printing Deepest : \n");
		for(i = 0 ; i <= dlevel+1; i++) {
			printf("Level : %d, %d, deepest_position = %d\n",i, deepest[i], (int32_t) (deepest_position[i]));	
		}
	#endif	
		
	// prepare_target Create target[]
	target = prepare_target(nlevel, dlevel, leaf_left, eviction_path_left, block_size, level, deepest, target_position);

	#ifdef ACCESS_CORAM_META_DEBUG
		printf("Printing Target : \n");
		for(i = 0 ; i <= dlevel+1; i++) {
			printf("%d , target_position = %d\n",target[i] , (int32_t) (target_position[i]));	
		}				
	#endif

	EvictOnceFast(deepest, target, deepest_position, target_position , eviction_path_left, path_hash, dlevel, nlevel, level, new_path_hash, leaf_left);
			
	#ifdef ACCESS_DEBUG			
		printf("\nLevel = %d, Eviction_path_left after Eviction:\n",level);
		if(level == recursion_levels)
			printf("Blocksize = %d\n", block_size);
		showPath_reverse(eviction_path_left, Z*(dlevel+1), data_size);
		print_stash_count(level,nlevel);
	#endif	

	#ifdef ENCRYPTION_ON
		encryptPath(eviction_path_left, encrypted_path, (Z*(dlevel+1)), data_size);
	#endif

	//time_report(5);			

	#ifdef ENCRYPTION_ON
		uploadPath(&rt, encrypted_path, path_size, leaf_left + nlevel, new_path_hash, new_path_hash_size, level, dlevel);		
	#else
		uploadPath(&rt, eviction_path_left, path_size, leaf_left + nlevel, new_path_hash, new_path_hash_size, level, dlevel);
	#endif		

	#ifndef PASSIVE_ADVERSARY
		new_path_hash_iter = new_path_hash;
		new_path_hash_trail = new_path_hash;
		old_path_hash_iter = path_hash;		
		new_path_hash_ptr = new_path_hash;
		leaf_temp_prev = (leaf_left+nlevel)<<1;
		#ifdef ENCRYPTION_ON
			path_ptr = encrypted_path;
		#else
			path_ptr = eviction_path_left;
		#endif
	
		for(i=0;i < ( Z * (dlevel+1) ); i++) {
			if(i%Z==0) {
				uint32_t p = i/Z;
				addToNewPathHash(path_ptr, old_path_hash_iter, new_path_hash_trail, new_path_hash_iter,(dlevel+1)-p, leaf_temp_prev, block_size, dlevel, level);
				leaf_temp_prev>>1;
				path_ptr+=(Z*block_size);
			}
				
		}
	#endif
		
	eviction_path_right = ReadBucketsFromPath(leaf_right + nlevel, path_hash, level);

	#ifdef ACCESS_DEBUG			
		printf("\nLevel = %d, leaf_right = %d, with + nlevel = %d, Eviction_path_right:\n", level, leaf_right, leaf_right + nlevel);
		showPath_reverse(eviction_path_right, Z*(dlevel+1), data_size);
		print_stash_count(level,nlevel);
	#endif
			
	//Eviction Path Right
	for(uint32_t e = 0; e <dlevel+2; e++){
		deepest_position[e] = -1;
		target_position[e] = -1;
	}

	deepest = prepare_deepest(nlevel, dlevel, leaf_right, eviction_path_right, block_size, level, deepest_position);
	target = prepare_target(nlevel, dlevel, leaf_right, eviction_path_right, block_size, level, deepest, target_position);		

	#ifdef ACCESS_CORAM_META_DEBUG
		printf("Printing Deepest : \n");
		for(i = 0 ; i <= dlevel+1; i++) {
			printf("Level : %d,%d, deepest_position = %d\n",i, deepest[i], (int32_t) (deepest_position[i]));	
		}
	#endif	

	#ifdef ACCESS_CORAM_META_DEBUG
		printf("Printing Target : \n");
		for(i = 0 ; i <= dlevel+1; i++) {
			printf("%d , target_position = %d\n",target[i] , (int32_t) (target_position[i]));	
		}				
	#endif			

	EvictOnceFast(deepest, target, deepest_position, target_position , eviction_path_right, path_hash, dlevel, nlevel, level, new_path_hash, leaf_right);
	#ifdef ACCESS_DEBUG			
		printf("\nLevel = %d, Eviction_path_right after Eviction:\n", level);
		showPath_reverse(eviction_path_right, Z*(dlevel+1), data_size);
		print_stash_count(level,nlevel);
	#endif	


	#ifdef ENCRYPTION_ON
		encryptPath(eviction_path_right, encrypted_path, (Z*(dlevel+1)), data_size);
	#endif

	#ifndef PASSIVE_ADVERSARY
		new_path_hash_iter = new_path_hash;
		new_path_hash_trail = new_path_hash;
		old_path_hash_iter = path_hash;		
		new_path_hash_ptr = new_path_hash;
		leaf_temp_prev = (leaf+nlevel)<<1;
		#ifdef ENCRYPTION_ON
			path_ptr = encrypted_path;
		#else
			path_ptr = eviction_path_right;
		#endif

		for(i=0;i < ( Z * (dlevel+1) ); i++) {
			if(i%Z==0) {
				uint32_t p = i/Z;
				addToNewPathHash(path_ptr, old_path_hash_iter, new_path_hash_trail, new_path_hash_iter,(dlevel+1)-p, leaf_temp_prev, block_size, dlevel, level);
				leaf_temp_prev>>1;
				path_ptr+=(Z*block_size);
			}
			
		}
	#endif


	#ifdef ENCRYPTION_ON
		uploadPath(&rt, encrypted_path, path_size, leaf_right + nlevel, path_hash, new_path_hash_size, level, dlevel);
	#else
		uploadPath(&rt, eviction_path_right, path_size, leaf_right + nlevel, path_hash, new_path_hash_size, level, dlevel);
	#endif			
	
	#ifdef SHOW_STASH_COUNT_DEBUG
		print_stash_count(level, nlevel);
	#endif

	time_report(4);
	
}

uint32_t CircuitORAM::CircuitORAM_Access(char opType, uint32_t id, uint32_t position_in_id, uint32_t leaf, uint32_t newleaf, uint32_t newleaf_nextlevel, unsigned char* decrypted_path, unsigned char* path_hash, uint32_t level, uint32_t dlevel, uint32_t nlevel, unsigned char* data_in, unsigned char *data_out){
	
	uint32_t tblock_size, tdata_size, i, k;
	uint8_t rt;									
	uint32_t return_value = 0;		
	if(recursion_levels!=-1) {
		if(level==recursion_levels) {
			tblock_size = data_size + ADDITIONAL_METADATA_SIZE;
			tdata_size = data_size;	
		} 
		else {
			tblock_size = recursion_data_size + ADDITIONAL_METADATA_SIZE;				
			tdata_size = recursion_data_size;			
		}
	} 
	else {
		tblock_size = data_size + ADDITIONAL_METADATA_SIZE;
		tdata_size = data_size;			
	}
			
	unsigned char* decrypted_path_ptr = decrypted_path;	
	uint32_t new_path_hash_size;
	unsigned char *new_path_hash_ptr;
	unsigned char *serialized_path, *serialized_path_ptr;
	uint32_t path_size = ORAMTree::Z * tblock_size * (dlevel+1);

	#ifndef PASSIVE_ADVERSARY
		new_path_hash_size = ((dlevel+1)*HASH_LENGTH);
	#else
		new_path_hash_size = 0;
	#endif

	#ifdef EXITLESS_MODE
		serialized_path = resp_struct->new_path;
		new_path_hash = resp_struct->new_path_hash;
	#endif	

	unsigned char *path_ptr;
	unsigned char *new_path_hash_iter = new_path_hash;
	unsigned char *new_path_hash_trail = new_path_hash;
	unsigned char *old_path_hash_iter = path_hash;		
	new_path_hash_ptr = new_path_hash;
	uint32_t leaf_temp_prev = (leaf+nlevel)<<1;

	//Pure Controller Time
	#ifdef TIME_PERFORMANCE
		time_report(3);
	#endif
            
	CircuitORAM_FetchBlock(decrypted_path, encrypted_path, serialized_result_block, path_hash, new_path_hash, path_size, new_path_hash_size, &return_value, leaf, newleaf, opType, id, position_in_id, newleaf_nextlevel, tdata_size, tblock_size, level, dlevel, nlevel, data_in, data_out);

	//Free Result_block (Application/Use result_block otherwise)
	#ifdef ACCESS_DEBUG
		printf("\nResult_block ID = %d, label = %d, Return value = %d\n",getId(serialized_result_block), 
			getTreeLabel(serialized_result_block),return_value);
		printf("Done with Fetch\n\n");
	#endif
            
	EvictionRoutine(decrypted_path, encrypted_path, path_hash, new_path_hash, tdata_size, 
			tblock_size, path_size, new_path_hash_size, leaf, level, dlevel, nlevel);

	return return_value;	
}

// ADIL.
uint32_t CircuitORAM::CircuitORAM_Access_Read(char opType, uint32_t id, uint32_t position_in_id, uint32_t leaf, uint32_t newleaf, uint32_t newleaf_nextlevel, unsigned char* decrypted_path, unsigned char* path_hash, uint32_t level, uint32_t dlevel, uint32_t nlevel, unsigned char* data_in, unsigned char *data_out){
	
	uint32_t tblock_size, tdata_size, i, k;
	uint8_t rt;									
	uint32_t return_value = 0;		
	if(recursion_levels!=-1) {
		if(level==recursion_levels) {
			tblock_size = data_size + ADDITIONAL_METADATA_SIZE;
			tdata_size = data_size;	
		} 
		else {
			tblock_size = recursion_data_size + ADDITIONAL_METADATA_SIZE;				
			tdata_size = recursion_data_size;			
		}
	} 
	else {
		tblock_size = data_size + ADDITIONAL_METADATA_SIZE;
		tdata_size = data_size;			
	}
			
	unsigned char* decrypted_path_ptr = decrypted_path;	
	uint32_t new_path_hash_size;
	unsigned char *new_path_hash_ptr;
	unsigned char *serialized_path, *serialized_path_ptr;
	uint32_t path_size = ORAMTree::Z * tblock_size * (dlevel+1);

	#ifndef PASSIVE_ADVERSARY
		new_path_hash_size = ((dlevel+1)*HASH_LENGTH);
	#else
		new_path_hash_size = 0;
	#endif

	#ifdef EXITLESS_MODE
		serialized_path = resp_struct->new_path;
		new_path_hash = resp_struct->new_path_hash;
	#endif	

	unsigned char *path_ptr;
	unsigned char *new_path_hash_iter = new_path_hash;
	unsigned char *new_path_hash_trail = new_path_hash;
	unsigned char *old_path_hash_iter = path_hash;		
	new_path_hash_ptr = new_path_hash;
	uint32_t leaf_temp_prev = (leaf+nlevel)<<1;

	//Pure Controller Time
	#ifdef TIME_PERFORMANCE
		time_report(3);
	#endif
            
	CircuitORAM_FetchBlock(decrypted_path, encrypted_path, serialized_result_block, path_hash, new_path_hash, path_size, new_path_hash_size, &return_value, leaf, newleaf, opType, id, position_in_id, newleaf_nextlevel, tdata_size, tblock_size, level, dlevel, nlevel, data_in, data_out);

	//Free Result_block (Application/Use result_block otherwise)
	#ifdef ACCESS_DEBUG
		printf("\nResult_block ID = %d, label = %d, Return value = %d\n",getId(serialized_result_block), 
			getTreeLabel(serialized_result_block),return_value);
		printf("Done with Fetch\n\n");
	#endif
            
  // ADIL. Don't evict.
	// EvictionRoutine(decrypted_path, encrypted_path, path_hash, new_path_hash, tdata_size, 
	//		tblock_size, path_size, new_path_hash_size, leaf, level, dlevel, nlevel);

  // ADIL. But the stash has to be emptied.

  // ADIL. clearing the stash (plus debugging)
  uint32_t stash_oc;
	if(recursion_levels != 0){
		stash_oc = recursive_stash[level].stashOccupancy();
	  printf("Level : %d, Before clearing :%d\n",level,stash_oc);
    recursive_stash[level].clear();
		stash_oc = recursive_stash[level].stashOccupancy();
		printf("Level : %d, After clearing  :%d\n",level,stash_oc);
	}			
	else{
		stash_oc = stash.stashOccupancy();
	  printf("Before clearing :%d\n",stash_oc);
    stash.clear();
		stash_oc = stash.stashOccupancy();
		printf("After clearing  :%d\n",stash_oc);
	}

	return return_value;	
}

uint32_t CircuitORAM::access(uint32_t id, uint32_t position_in_id, char opType, uint8_t level, unsigned char* data_in, unsigned char* data_out, uint32_t* prev_sampled_leaf){
	uint32_t leaf = 0;
	uint32_t nextLeaf;
	uint32_t id_adj;				
	uint32_t newleaf;
	uint32_t newleaf_nextlevel = -1;
	unsigned char random_value[ID_SIZE_IN_BYTES];

	if(recursion_levels ==  -1) {
		sgx_status_t rt = SGX_SUCCESS;
		rt = sgx_read_rand((unsigned char*) random_value,ID_SIZE_IN_BYTES);
		uint32_t newleaf = *((uint32_t *)random_value) % N;

		if(oblivious_flag) {
			oarray_search2(posmap,id,&leaf,newleaf,max_blocks);		
		}
		else{
			leaf = posmap[id];
			posmap[id] = newleaf;			
		}	
		time_report(1);	
	
		decrypted_path = ReadBucketsFromPath(leaf+N, path_hash,-1);			

    // ADIL.
    if (opType == 'r') {
      printf("[ADIL]. Simple (LogN) reading\n");
		  CircuitORAM_Access_Read(opType, id, -1, leaf, newleaf, -1, decrypted_path, path_hash, -1, D, N, data_in, data_out);
    } else {
		  CircuitORAM_Access(opType, id, -1, leaf, newleaf, -1, decrypted_path, path_hash, -1, D, N, data_in, data_out);
    }
	}

	else if(level==0) {
		sgx_read_rand((unsigned char*) random_value, ID_SIZE_IN_BYTES);
		//To slot into one of the buckets of next level
		newleaf = *((uint32_t *)random_value) % (N_level[level+1]);
		*prev_sampled_leaf = newleaf;

		if(oblivious_flag) {
			oarray_search2(posmap, id, &leaf, newleaf, real_max_blocks_level[level]);				
		}			
		else {
			leaf = posmap[id];
			posmap[id] = newleaf;
		}

		#ifdef ACCESS_DEBUG
			printf("access : Level = %d: \n Requested_id = %d, Corresponding leaf from posmap = %d, Newleaf assigned = %d,\n\n",level,id,leaf,newleaf);
		#endif				
		return leaf;
	}
	else if(level == 1){
		id_adj = id/x;
		leaf = access(id, -1, opType, level-1, data_in, data_out, prev_sampled_leaf);
		

		//sampling leafs for a level ahead		
		sgx_read_rand((unsigned char*) random_value, ID_SIZE_IN_BYTES);
		newleaf_nextlevel = *((uint32_t *)random_value) % N_level[level+1];					

		#ifdef ACCESS_DEBUG
			printf("access : Level = %d: \n leaf = %d, block_id = %d, position_in_id = %d, newleaf_nextlevel = %d\n",level,leaf,id,position_in_id,newleaf_nextlevel);
		#endif

		nextLeaf = access_oram_level(opType, leaf, id, position_in_id, level, *prev_sampled_leaf, newleaf_nextlevel, data_in, data_out);
		*prev_sampled_leaf = newleaf_nextlevel;

		#ifdef ACCESS_DEBUG
			printf("access, Level = %d (After ORAM access): nextLeaf from level = %d \n\n",level,nextLeaf);
		#endif
		return nextLeaf;
	}
	else if(level == recursion_levels){
		//DataAccess for leaf.
		id_adj = id/x;
		position_in_id = id%x;
		leaf = access(id_adj, position_in_id, opType, level-1, data_in, data_out, prev_sampled_leaf);	
		#ifdef ACCESS_DEBUG					
			printf("access, Level = %d:  before access_oram_level : Block_id = %d, Newleaf = %d, Leaf from level = %d, Flag = %d\n",level,id,*prev_sampled_leaf,leaf,oblivious_flag);
		#endif
		//ORAM ACCESS of recursion_levels is to fetch entire Data block, no position_in_id, hence -1)
		time_report(1);
		access_oram_level(opType, leaf, id, -1, level, *prev_sampled_leaf, -1, data_in, data_out);
		return 0;	
	}
	else{
		id_adj = id/x;
		uint32_t nl_position_in_id = id%x;
		leaf = access(id_adj, nl_position_in_id, opType, level-1, data_in, data_out, prev_sampled_leaf);	
		//sampling leafs for a level ahead
		//random_value = (unsigned char*) malloc(size);					
		sgx_read_rand((unsigned char*) random_value, ID_SIZE_IN_BYTES);
		newleaf_nextlevel = *((uint32_t *)random_value) % N_level[level+1];
		newleaf = *prev_sampled_leaf;					
		*prev_sampled_leaf = newleaf_nextlevel;
		//free(random_value);
		#ifdef ACCESS_DEBUG
			printf("access, Level = %d :\n leaf = %d, block_id = %d, position_in_id = %d, newLeaf = %d, newleaf_nextlevel = %d\n",level,leaf,id,position_in_id,newleaf,newleaf_nextlevel);
		#endif										
		nextLeaf = access_oram_level(opType, leaf, id, position_in_id, level, newleaf, newleaf_nextlevel, data_in, data_out);
		#ifdef ACCESS_DEBUG
			printf("access, Level = %d : \n nextLeaf from level = %d\n",level,nextLeaf);
		#endif
	return nextLeaf;

	}

return nextLeaf;
}	


void CircuitORAM::Access_temp(uint32_t id, char opType, unsigned char* data_in, unsigned char* data_out){
	uint32_t prev_sampled_leaf=-1;
	access(id, -1, opType, recursion_levels, data_in, data_out, &prev_sampled_leaf);
}

void CircuitORAM::Create(){
	printf("WE GOT HERE!\n");
}

void CircuitORAM::Access(){
	printf("WE GOT HERE TOO!\n");
}


/*
void CircuitORAM::Create(uint8_t pZ, uint32_t pmax_blocks, uint32_t pdata_size, uint32_t pstash_size, uint32_t poblivious_flag, uint32_t precursion_data_size, int8_t precursion_levels, uint64_t onchip_posmap_mem_limit){
	printf("WE GOT HERE!\n");
	//Initialize(pZ, pmax_blocks, pdata_size, pstash_size, poblivious_flag, precursion_data_size, precursion_levels, onchip_posmap_mem_limit);
}

void CircuitORAM::Access(uint32_t id, char opType, unsigned char* data_in, unsigned char* data_out){
	uint32_t prev_sampled_leaf=-1;
	access(id, -1, opType, recursion_levels, data_in, data_out, &prev_sampled_leaf);
}

*/
