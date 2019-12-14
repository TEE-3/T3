#include <string.h>
#include <vector>
#include "../Globals_Enclave.hpp"
#include "../ORAMTree.hpp"
#include "../PathORAM_Enclave.hpp"
#include "../CircuitORAM_Enclave.hpp"
#include "../reducer/reducer.hpp"

extern std::vector<PathORAM *> poram_instances;
extern std::vector<CircuitORAM *> coram_instances;

unsigned short lfsr = 0xACE1u;
unsigned bit;

unsigned rand()
{
  bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
  return lfsr =  (lfsr >> 1) | (bit << 15);
}

void generate_random_data(unsigned char* data, size_t size)
{
  for (int i = 0; i < size; i++) {
    data[i] = rand() % 256;
  }
}

// ADIL. This function is solely for testing the ORAM protocol
void ORAM_testing(uint32_t instance_id, uint8_t oram_type, uint32_t ORAM_block_size)
{
  printf("[SGX] ORAM testing has begun\n");

	//TODO : Would be nice to remove this dynamic allocation.
	PathORAM *poram_current_instance;
	CircuitORAM *coram_current_instance;
	uint32_t id;	

  unsigned char data_in[ORAM_block_size];
  unsigned char data_out[ORAM_block_size];
  unsigned char data_temp[ORAM_block_size];
	
	if(oram_type==0){
    printf("[SGX] Testing Path ORAM\n");
		poram_current_instance = poram_instances[instance_id];
    for (int i=0; i<400; i++) {
      printf("Index: %d\n", i);
      memset(data_in, 0, ORAM_block_size);
      memset(data_out, 0, ORAM_block_size);
      generate_random_data((unsigned char*) &data_in, ORAM_block_size);

      hexdump((unsigned char*) &data_in, ORAM_block_size);

      // WRITE first and then READ
		  poram_current_instance->Access_temp(id, 'w', (unsigned char*) &data_in, (unsigned char*) &data_temp); 
		  poram_current_instance->Access_temp(id, 'r', (unsigned char*) &data_temp, (unsigned char*) &data_out);

      // TESTING
      check_data((unsigned char*) &data_in, (unsigned char*) &data_out, ORAM_block_size);
   }
	}
  printf("[SUCCESS] ORAM-TEST COMPLETE!\n");
}