/*
   Cache Simulator
   Level one L1 and level two L2 cache parameters are read from file
   (block size, line per set and set per cache).
   The 32 bit address is divided into:
   -tag bits (t)
   -set index bits (s)
   -block offset bits (b)

   s = log2(#sets)   b = log2(block size)  t=32-s-b
*/

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <iomanip>
#include <stdlib.h>
#include <cmath>
#include <bitset>
#include <array>


using namespace std;
//access state:
#define NA 0 // no action
#define RH 1 // read hit
#define RM 2 // read miss
#define WH 3 // Write hit
#define WM 4 // write miss


struct config{
  int L1blocksize;
  int L1setsize;
  int L1size;
  int L2blocksize;
  int L2setsize;
  int L2size;
};

class Cache{
  public: 
    int t,s,b;

    void init(int blocksize, int setsize, int size){
      b = log2(blocksize);
      //log2(size * Ki / blocksize / setsize) = log(size) + log(Ki) - log(blocksize) - log(setsize)
      //s = setsize == 1 ? log2(size) + 10 - log2(blocksize) : log2(size) + 10 - log2(blocksize) - log2(setsize);
      if(setsize == 0){ //Fully Associative
        s = 0;
      }else if(setsize == 1){ // Direct Mapping
        s = log2(size) + 10 - log2(blocksize);
      }else{
        s = log2(size) + 10 - log2(blocksize) - log2(setsize);
      }
      t = 32 - s - b;
    }
};    

int main(int argc, char* argv[]){



  config cacheconfig;
  ifstream cache_params;
  string dummyLine;
  cache_params.open(argv[1]);
  
  while(!cache_params.eof())  // read config file
  {
    cache_params>>dummyLine;
    cache_params>>cacheconfig.L1blocksize;
    cache_params>>cacheconfig.L1setsize;              
    cache_params>>cacheconfig.L1size;
    cache_params>>dummyLine;              
    cache_params>>cacheconfig.L2blocksize;           
    cache_params>>cacheconfig.L2setsize;        
    cache_params>>cacheconfig.L2size;
  }

  Cache L1_Cache;
  Cache L2_Cache;
  L1_Cache.init(cacheconfig.L1blocksize, cacheconfig.L1setsize, cacheconfig.L1size);
  L2_Cache.init(cacheconfig.L2blocksize, cacheconfig.L2setsize, cacheconfig.L2size);

  if(cacheconfig.L1setsize == 0){
    cacheconfig.L1setsize = 1024 * (cacheconfig.L1size / cacheconfig.L1blocksize);
  }

  if(cacheconfig.L2setsize == 0){
    cacheconfig.L2setsize = 1024 * (cacheconfig.L2size / cacheconfig.L2blocksize);
  }

  int line_L1 = (int)pow(2,L1_Cache.s);
  int line_L2 = (int)pow(2,L2_Cache.s);

  int tag_arr_L1[line_L1][cacheconfig.L1setsize];
  int tag_arr_L2[line_L2][cacheconfig.L2setsize];
  for(int i = 0;i < line_L1;i++){
    for(int j = 0;j< cacheconfig.L1setsize;j++){
      tag_arr_L1[i][j] = 0;
    }
  }
  for(int i = 0;i < line_L2;i++){
    for(int j = 0;j< cacheconfig.L2setsize;j++){
      tag_arr_L2[i][j] = 0;
    }
  }

  int valid_L1[line_L1][cacheconfig.L1setsize];
  int valid_L2[line_L2][cacheconfig.L2setsize];
  for(int i = 0;i < line_L1;i++){
    for(int j = 0;j< cacheconfig.L1setsize;j++){
      valid_L1[i][j] = 0;
    }
  }
  for(int i = 0;i < line_L2;i++){
    for(int j = 0;j< cacheconfig.L2setsize;j++){
      valid_L2[i][j] = 0;
    }
  }

  //Initialize Dirty bit
  int dirty_L1[line_L1][cacheconfig.L1setsize];
  int dirty_L2[line_L2][cacheconfig.L2setsize];
  for(int i = 0;i < line_L1;i++){
    for(int j = 0;j< cacheconfig.L1setsize;j++){
      dirty_L1[i][j] = 0;
    }
  }
  for(int i = 0;i < line_L2;i++){
    for(int j = 0;j< cacheconfig.L2setsize;j++){
      dirty_L2[i][j] = 0;
    }
  }

  //Initialize Counter
  int counterL1[line_L1];
  int counterL2[line_L2];
  for(int i = 0;i < line_L1;i++){
    counterL1[i] = 0;
  }
  for(int i = 0;i < line_L2;i++){
    counterL2[i] = 0;
  }

  int L1AcceState =0; // L1 access state variable, can be one of NA, RH, RM, WH, WM;
  int L2AcceState =0; // L2 access state variable, can be one of NA, RH, RM, WH, WM;

  ifstream traces;
  ofstream tracesout;
  string outname;
  outname = string(argv[2]) + ".out";

  traces.open(argv[2]);
  tracesout.open(outname.c_str());

  string line;
  string accesstype;  // the Read/Write access type from the memory trace;
  string xaddr;       // the address from the memory trace store in hex;
  unsigned int addr;  // the address from the memory trace store in unsigned int;        
  bitset<32> accessaddr; // the address from the memory trace store in the bitset;


  int cycle = 0;
  if (traces.is_open()&&tracesout.is_open()){    
    while (getline (traces,line)){   // read mem access file and access Cache

      istringstream iss(line); 
      if (!(iss >> accesstype >> xaddr)) {break;}
      stringstream saddr(xaddr);
      saddr >> std::hex >> addr;
      accessaddr = bitset<32> (addr);

      uint32_t tag1 = (bitset<32>((accessaddr.to_string().substr(0,L1_Cache.t))).to_ulong());
      uint32_t index1 = 0;
      if(cacheconfig.L1setsize != 0){
        index1 = (bitset<32>((accessaddr.to_string().substr(L1_Cache.t,L1_Cache.s))).to_ulong());
      }
      uint32_t offset1 = (bitset<32>((accessaddr.to_string().substr(L1_Cache.t+L1_Cache.s,L1_Cache.b))).to_ulong());
      uint32_t tag2 = (bitset<32>((accessaddr.to_string().substr(0,L2_Cache.t))).to_ulong());
      uint32_t index2 = 0;
      if(cacheconfig.L2setsize != 0){
        index2 = (bitset<32>((accessaddr.to_string().substr(L2_Cache.t,L2_Cache.s))).to_ulong());
      }
      uint32_t offset2 = (bitset<32>((accessaddr.to_string().substr(L2_Cache.t+L2_Cache.s,L2_Cache.b))).to_ulong());

      //reset access state
      L1AcceState = 0;
      L2AcceState = 0;
      

      // access the L1 and L2 Cache according to the trace;
      if (accesstype.compare("R")==0){   

        //Check if L1 cache hit
        for(int i = 0; i < cacheconfig.L1setsize;i++){
          if(tag_arr_L1[index1][i] == tag1 && valid_L1[index1][i] == 1){
            L1AcceState = 1;
            break;
          }
        }

        //L1 cache read has missed,now checking L2
        if(L1AcceState == 0){
          L1AcceState = 2;
          for(int i = 0; i < cacheconfig.L2setsize; i++){
            if((tag_arr_L2[index2][i] == tag2) && (valid_L2[index2][i] == 1)){
              L2AcceState = 1; //L2 hit
              //Look for empty room in L1
              int hasRoom = 0;
              for(int j = 0; j < cacheconfig.L1setsize; j++){
                if(valid_L1[index1][j] == 0){
                  tag_arr_L1[index1][j] = tag1;
                  valid_L1[index1][j] = 1;
                  dirty_L1[index1][j] = 0;
                  hasRoom = 1;
                  break;
                }
              }
              //There is no empty room in L1, we have to evict
              if(!hasRoom){
                //if the block we are evicting is dirty, we need to write back before eviction
                if(dirty_L1[index1][counterL1[index1] == 1]){
                  //search for tag in L2
                  int dirty_addr_int = ((tag_arr_L1[index1][counterL1[index1]]) << (32 - L1_Cache.t)) | (index1 << L1_Cache.b) | offset2;
                  bitset<32> dirty_addr= bitset<32>(dirty_addr_int);
                  int dirty_tag = (bitset<32>((dirty_addr.to_string().substr(0,L2_Cache.t))).to_ulong());
                  for(int j = 0; j < cacheconfig.L2setsize; j++){
                    if((tag_arr_L2[index2][j] == dirty_tag) && (valid_L2[index2][j] == 1)){
                      dirty_L2[index2][j] = 1; //update the block in L2
                      break;
                    }
                  }
                }
                //At this point we could have a write miss on L2
                //Now we've finished write back, do the eviction
                tag_arr_L1[index1][counterL1[index1]] = tag1;
                dirty_L1[index1][counterL1[index1]] = 0;
                counterL1[index1]++;
                //reset counter if it reaches max way
                if(counterL1[index1] == cacheconfig.L1setsize) counterL1[index1] = 0;
              }
              break;
            }
          }
          //L2 read miss
          if(L2AcceState == 0){
            L2AcceState = 2;
            int hasRoom = 0;
            //search for empty room in L2
            for(int i = 0;i < cacheconfig.L2setsize;i++){
              if(valid_L2[index2][i] == 0){
                tag_arr_L2[index2][i] = tag2;
                valid_L2[index2][i] = 1;
                dirty_L2[index2][i] = 0;
                hasRoom = 1;
                break;
              }
            }
            //No empty space, we have to evict
            if(!hasRoom){
              //we should check dirty and perform WB at this point but this L2 is the lowest level,
              //we can ignore that and simply do eviction
              
              //invalidate L1 block
              int invalid_addr_int = ((tag_arr_L2[index2][counterL2[index2]]) << (32 - L2_Cache.t)) | (index2 << L2_Cache.b) | offset2;
              int invalid_index = 0;
              bitset<32> invalid_addr= bitset<32>(invalid_addr_int);
              if(cacheconfig.L1setsize != 0){
                invalid_index = (bitset<32>((invalid_addr.to_string().substr(L1_Cache.t,L1_Cache.s))).to_ulong());
              }
              int invalid_tag = (bitset<32>((invalid_addr.to_string().substr(0,L1_Cache.t))).to_ulong());
              for(int i = 0; i < cacheconfig.L1setsize; i++){
                if(tag_arr_L1[invalid_index][i] == invalid_tag){
                  valid_L1[invalid_index][i] = 0; 
                  break;
                }
              }
              
              tag_arr_L2[index2][counterL2[index2]] = tag2;
              dirty_L2[index2][counterL2[index2]] = 0;

              counterL2[index2]++;
              //reset L2 counter if necessary
              if(counterL2[index2] == cacheconfig.L2setsize) counterL2[index2] = 0;
            }
            //reload L1 cache
            hasRoom = 0;
            for(int i = 0; i < cacheconfig.L1setsize;i++){
              if(valid_L1[index1][i] == 0){
                tag_arr_L1[index1][i] = tag1;
                valid_L1[index1][i] = 1;
                dirty_L1[index1][i] = 0;
                hasRoom = 1;
                break;
              }
            }
            if(!hasRoom){
                //if the block we are evicting is dirty, we need to write back before eviction
                if(dirty_L1[index1][counterL1[index1] == 1]){
                  //search for tag in L2
                  int dirty_addr_int = ((tag_arr_L1[index1][counterL1[index1]]) << (32 - L1_Cache.t)) | (index1 << L1_Cache.b) | offset2;
                  bitset<32> dirty_addr= bitset<32>(dirty_addr_int);
                  int dirty_tag = (bitset<32>((dirty_addr.to_string().substr(0,L2_Cache.t))).to_ulong());
                  for(int j = 0; j < cacheconfig.L2setsize; j++){
                    if((tag_arr_L2[index2][j] == dirty_tag) && (valid_L2[index2][j] == 1)){
                      dirty_L2[index2][j] = 1; //update the block in L2
                      break;
                    }
                  }
                }
                //At this point we could have a write miss on L2
                //Now we've finished write back, do the eviction
                tag_arr_L1[index1][counterL1[index1]] = tag1;
                dirty_L1[index1][counterL1[index1]] = 0;
                counterL1[index1]++;
                //reset counter if it reaches max way
                if(counterL1[index1] == cacheconfig.L1setsize) counterL1[index1] = 0;
              }
          }
        }
      }


      else {    
        for(int i = 0; i < cacheconfig.L1setsize;i++){
          //if we have write hit on L1
          if(tag_arr_L1[index1][i] == tag1 && valid_L1[index1][i] == 1){
            L1AcceState = 3;
            L2AcceState = 0;
            dirty_L1[index1][i] = 1;
            break;
          }
        }
        // if we have write miss on L1, since no allocate,forward to L2
        if(L1AcceState== 0){
          L1AcceState = 4;
          for(int i = 0; i < cacheconfig.L2setsize;i++){
            //L2 write hit
            if(tag_arr_L2[index2][i] == tag2 && valid_L2[index2][i] == 1){
              L2AcceState = 3;
              dirty_L2[index2][i] = 1;
              break;
            }
          }
          //if L2 misses
          if(L2AcceState == 0){
            L2AcceState = 4;
          }
        }
      }

      cycle++;

      // Output hit/miss results for L1 and L2 to the output file
      tracesout<< L1AcceState << " " << L2AcceState << endl;


    }
    traces.close();
    tracesout.close(); 
  }
  else cout<< "Unable to open trace or traceout file ";

  return 0;
}
