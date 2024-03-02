# Assignment 2 - Buffer Manager

This implementation just follows the Assignment 2's guide. All interfaces in buffer_mgr.h are implemented.

-----------------------------------------------------------------------------------------------------------------
## Implementation brief
All file related methods use only Standard C file functions to enusre maximum compatibility. Since this toy DBS is too simple, some advanced file opearting feature are no requried.

storage_mgr is strongly used for file storage.

Use only simple linear search for page to frame search, no hash function used.

Implementation is too straightforward, no tricky part contained.

Implemented two replacement algorithms FIFO and LRU.
All test cases are passed, no memory access and memory leak errors caught by 'valgrind'.
-----------------------------------------------------------------------------------------------------------------

## Run and test:
1. Enter assign2 path
2. Type and Enter 'make test_assign2_1'
3. Run by type and enter './test_assign2_1'
4. Run by type and enter 'valgrind ./test_assign2_1'

5. Type and Enter 'make test_assign2_2'
6. Run by type and enter './test_assign2_2'
7. Run by type and enter 'valgrind ./test_assign2_2'

All test cases are passed.-----------------------------------------------------------------------------------

## Credits: 

All by *Guanan Xiang*.