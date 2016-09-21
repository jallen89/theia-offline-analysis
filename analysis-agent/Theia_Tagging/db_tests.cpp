//build:
// g++ -g -std=c++0x -Wall -o db_test db_tests.cpp TheiaDB.cpp -lpqxx -lpq

#include "TheiaDB.h"                                                             
                                                                                 
int main() {                                                                     
  query_entry_postgres(1000000,1000010,"123.222.232.100:4321");                  
  return 0;                                                                      
}
