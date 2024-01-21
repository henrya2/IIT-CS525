# Assignment 1 - Storage Manager 

This implementation just follows the Assignment 1's guide. All interfaces in storage_mgr.h are implemented.

-----------------------------------------------------------------------------------------------------------------
## Implementation brief
All file related methods use only Standard C file functions to enusre maximum compatibility. Since this toy DBS is too simple, some advanced file opearting feature are no requried.

I store FILE* to the SM_FileHandle's mgtInfo, it is a simple optimization that reopen file is avoided. Since Open and Close file are expensive due to their usage of several kernel switch. So caching the FILE* is a good way to improve performance.

I use a global static variable initPageContent which contains a PAGE_SIZE '\0', it is simple optimization for write a block of empty page. Use loop to write one byte by a time is really slow.

Since read and write a fixed size data of file are too straightforward, there is nothing to mentioned.
-----------------------------------------------------------------------------------------------------------------

## Run and test:
1. Enter assign1 path
2. Type and Enter 'make test_assign1'
3. Run by type and enter './test_assign1'

All test cases are passed.
-----------------------------------------------------------------------------------------------------------------

## Credits: 

All by *Guanan Xiang*.
