#include <string.h>
#include <vector>
#include "../Globals_Enclave.hpp"
#include "../ORAMTree.hpp"
#include "../PathORAM_Enclave.hpp"
#include "../CircuitORAM_Enclave.hpp"
#include "../reducer/reducer.hpp"

extern std::vector<PathORAM *> poram_instances;
extern std::vector<CircuitORAM *> coram_instances;

#define TEST_NUM 50000
#define ID 1

// This function is for testing the ORAM input/output consistency
void ORAM_testing(uint32_t instance_id, uint8_t oram_type, uint32_t ORAM_block_size)
{
  printf("[SGX] ORAM testing has begun\n");

	PathORAM *poram_current_instance;
	CircuitORAM *coram_current_instance;
  
  uint32_t utxos_per_block = ORAM_block_size/68;

    unsigned char data_out[ORAM_block_size];
    unsigned char trash[ORAM_block_size];

    unsigned char data_temp[ORAM_block_size];
    memset(&data_temp[0],255,68);
	
	if(oram_type==0){
    printf("[SGX] Testing Path ORAM\n");
		poram_current_instance = poram_instances[instance_id];
    
    //Write the initial block content: first 68 bytes with 0xff, then random data
	  poram_current_instance->Access_temp(ID, 'w', (unsigned char*) &data_temp, (unsigned char*) &trash); 
    
    for (int i=1; i<TEST_NUM && i<utxos_per_block; i++) {
      printf("INDEX: %d\n", i);

      // Each READ has to be consistent with data_temp
		  poram_current_instance->Access_temp(ID, 'z', (unsigned char*) &trash, (unsigned char*) &data_out); 
      // hexdump((unsigned char*) &data_out, ORAM_block_size);
      check_data((unsigned char*) &data_temp, (unsigned char*) &data_out, ORAM_block_size);
      
      //Modify block with another 68 bytes with 0xff and save a copy on data_temp
      memset(&data_out[i*68],255,68);
		  memcpy((unsigned char*) &data_temp, (unsigned char*) &data_out, ORAM_block_size);
      poram_current_instance->Access_temp(ID, 'w', (unsigned char*) &data_out, (unsigned char*) &trash); 

    }
	
  }else{
    printf("[SGX] Testing Circuit ORAM\n");
		coram_current_instance = coram_instances[instance_id];
    
    //Write the initial block content: first 68 bytes with 0xff, then random data
	  coram_current_instance->Access_temp(ID, 'w', (unsigned char*) &data_temp, (unsigned char*) &trash); 
    
    for (int i=1; i<TEST_NUM && i<utxos_per_block; i++) {
      printf("INDEX: %d\n", i);

      // Each READ has to be consistent with data_temp
		  coram_current_instance->Access_temp(ID, 'z', (unsigned char*) &trash, (unsigned char*) &data_out); 
      // hexdump((unsigned char*) &data_out, ORAM_block_size);
      check_data((unsigned char*) &data_temp, (unsigned char*) &data_out, ORAM_block_size);
      
      //Modify block with another 68 bytes with 0xff and save a copy on data_temp
      memset(&data_out[i*68],255,68);
		  memcpy((unsigned char*) &data_temp, (unsigned char*) &data_out, ORAM_block_size);
      coram_current_instance->Access_temp(ID, 'w', (unsigned char*) &data_out, (unsigned char*) &trash); 

    }    
  }
  printf("[SUCCESS] ORAM-TEST COMPLETE!\n");
}