
extern "C" {
#include "ssd.h"
#include "ftl.h"
#include "ftl_obj_strategy.h"
#include "ftl_sect_strategy.h"
#include "uthash.h"
}
extern "C" int g_init;
extern "C" int g_server_create;
extern "C" int g_init_log_server;

#define GTEST_DONT_DEFINE_FAIL 1
#include <gtest/gtest.h>

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <math.h>

using namespace std;

namespace {
    class ObjectUnitTest : public ::testing::TestWithParam<size_t> {
        public:
            virtual void SetUp() {
                size_t mb = 512;//GetParam();
                pages_= mb * (1048576 / 4096); // number_of_pages = disk_size (in MB) * 1048576 / page_size
                size_t block_x_flash = pages_ / 8; // all_blocks_on_all_flashes = number_of_pages / pages_in_block
                size_t flash = block_x_flash / 4096; // number_of_flashes = all_blocks_on_all_flashes / number_of_blocks_in_flash
                ofstream ssd_conf("data/ssd.conf", ios_base::out | ios_base::trunc);
                ssd_conf << "FILE_NAME ./data/ssd.img\n"
                    "PAGE_SIZE 4096\n"
                    "PAGE_NB 10\n" // 8 pages per block +2 pages over-provision = 125% of disk size
                    "SECTOR_SIZE 1\n"
                    "FLASH_NB " << flash << "\n" // see calculations above
                    "BLOCK_NB 4096\n"
                    "PLANES_PER_FLASH 1\n"
                    "REG_WRITE_DELAY 82\n"
                    "CELL_PROGRAM_DELAY 900\n"
                    "REG_READ_DELAY 82\n"
                    "CELL_READ_DELAY 50\n"
                    "BLOCK_ERASE_DELAY 2000\n"
                    "CHANNEL_SWITCH_DELAY_R 16\n"
                    "CHANNEL_SWITCH_DELAY_W 33\n"
                    "CHANNEL_NB 4\n"
                    "STAT_TYPE 15\n"
                    "STAT_SCOPE 62\n"
                    "STAT_PATH /tmp/stat.csv\n"
                    "STORAGE_STRATEGY 2\n"; // object strategy
                ssd_conf.close();
                SSD_INIT();
                object_size_ = GetParam();
                int object_pages = (int)ceil(1.0 * object_size_ / PAGE_SIZE); // ceil because we can't have a page belong to 2 objects
                objects_in_ssd_ = (unsigned int)(PAGES_IN_SSD / object_pages);
            }
            virtual void TearDown() {
                SSD_TERM();
                remove("data/empty_block_list.dat");
                remove("data/inverse_block_mapping.dat");
                remove("data/inverse_page_mapping.dat");
                remove("data/mapping_table.dat");
                remove("data/valid_array.dat");
                remove("data/victim_block_list.dat");
                remove("data/ssd.conf");
                g_init = 0;
                g_server_create = 0;
                g_init_log_server = 0;
            }
        protected:
            size_t pages_;
            int object_size_;
            unsigned int objects_in_ssd_;
    }; // OccupySpaceStressTest

    INSTANTIATE_TEST_CASE_P(DiskSize, ObjectUnitTest, ::testing::Values(2048, // 1/2 page
                                                                        6144, // 1 1/2 pages
                                                                        2 * 1024 * 1024, // 2 MB
                                                                        6 * 1024 * 1024)); // 6 MB

    TEST_P(ObjectUnitTest, SimpleObjectCreate) {
        printf("SimpleObjectCreate test started\n");
        printf("Page no.:%ld\nPage size:%d\n",PAGES_IN_SSD,PAGE_SIZE);
        printf("Object size: %d bytes\n",object_size_);
        // Fill the disk with objects
        for(unsigned int p=0; p < objects_in_ssd_; p++){
            //printf("%ld/%ld\n",(long)p,PAGES_IN_SSD);
            ASSERT_LT(0, _FTL_OBJ_CREATE(object_size_));
        }
        // At this step there shouldn't be any free page
        ASSERT_EQ(FAIL, _FTL_OBJ_CREATE(object_size_));      
        printf("SimpleObjectCreate test ended\n");
    }

    TEST_P(ObjectUnitTest, SimpleObjectCreateWrite) {
        printf("SimpleObjectCreateWrite test started\n");
        printf("Page no.:%ld\nPage size:%d\n",PAGES_IN_SSD,PAGE_SIZE);
        printf("Object size: %d bytes\n",object_size_);

        // used to keep all the assigned ids
        int objects[objects_in_ssd_];

        // Fill 50% of the disk with objects
        for(unsigned int p=0; p < objects_in_ssd_ / 2; p++){
            int new_obj = _FTL_OBJ_CREATE(object_size_);
            ASSERT_LT(0, new_obj);
            objects[p] = new_obj;
        }
        // Write PAGE_SIZE data to each one
        for(unsigned int p=0; p < objects_in_ssd_/2; p++){
            ASSERT_EQ(SUCCESS, _FTL_OBJ_WRITE(objects[p],0,PAGE_SIZE));
        }
        printf("SimpleObjectCreateWrite test ended\n");
    }

    TEST_P(ObjectUnitTest, SimpleObjectCreateRead) {
        printf("SimpleObjectCreateRead test started\n");
        printf("Page no.:%ld\nPage size:%d\n",PAGES_IN_SSD,PAGE_SIZE);
        printf("Object size: %d bytes\n",object_size_);

        // used to keep all the assigned ids
        int objects[objects_in_ssd_];

        // Fill 50% of the disk with objects
        for(unsigned int p=0; p < objects_in_ssd_/2; p++){
            int new_obj = _FTL_OBJ_CREATE(object_size_);
            ASSERT_LT(0, new_obj);
            objects[p] = new_obj;
        }
        // Read PAGE_SIZE data from each one
        for(unsigned int p=0; p < objects_in_ssd_/2; p++){
            ASSERT_EQ(SUCCESS, _FTL_OBJ_READ(objects[p],0,PAGE_SIZE));
        }
        printf("SimpleObjectCreateRead test ended\n");
    }
    
    TEST_P(ObjectUnitTest, SimpleObjectCreateDelete) {
        printf("SimpleObjectCreateDelete test started\n");
        printf("Page no.:%ld\nPage size:%d\n",PAGES_IN_SSD,PAGE_SIZE);
        printf("Object size: %d bytes\n",object_size_);
        
        // used to keep all the assigned ids
        int objects[objects_in_ssd_];
        
        // Fill the disk with objects
        for(unsigned int p=0; p < objects_in_ssd_; p++){
            int new_obj = _FTL_OBJ_CREATE(object_size_);
            ASSERT_LT(0, new_obj);
            objects[p] = new_obj;
        }
        
        // Now make sure we can't create a new object, aka the disk is full
        ASSERT_EQ(FAIL, _FTL_OBJ_CREATE(object_size_)); 
        
        // Delete all objects
        for (unsigned int p=0; p < objects_in_ssd_; p++) {
            ASSERT_EQ(SUCCESS, _FTL_OBJ_DELETE(objects[p]));
        }
        
        // And try to fill the disk again with the same number of sized objects
        for(unsigned int p=0; p < objects_in_ssd_; p++){
            ASSERT_LT(0, _FTL_OBJ_CREATE(object_size_));
        }
        
        printf("SimpleObjectCreateDelete test ended\n");
    }

    TEST_P(ObjectUnitTest, ObjectGrowthTest) {
        printf("ObjectGrowth test started\n");
        printf("Page no.:%ld\nPage size:%d\n",PAGES_IN_SSD,PAGE_SIZE);
        printf("Object size: %d bytes\n",object_size_);
        
        // create an object_size_bytes_ - sized object
        int obj_id = _FTL_OBJ_CREATE(object_size_);
        ASSERT_LT(0, obj_id);
        
        unsigned int size = 0;

        // continuously extend it with object_size_bytes_ chunks
        while (size / PAGE_SIZE < PAGES_IN_SSD) {
            ASSERT_EQ(SUCCESS, _FTL_OBJ_WRITE(obj_id, size, object_size_));
            size += object_size_;
        }

        // we should've covered the whole disk by now, so another write should fail
        ASSERT_EQ(FAIL, _FTL_OBJ_WRITE(obj_id, size, object_size_)); 
        
        printf("ObjectGrowth test ended\n");
    }
} //namespace
