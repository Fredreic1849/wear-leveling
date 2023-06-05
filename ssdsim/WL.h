int total_erase_count(struct ssd_info *ssd);
double single_block_utilization(struct ssd_info *ssd, int block_id);
int check_variance(double var);
double calculate_variance(struct ssd_info *ssd);
double calculate_mean(struct ssd_info *ssd);
void freeze_highest_erase_count_blocks(struct ssd_info * ssd, int n);
void unfreeze_highest_erase_count_blocks(struct ssd_info * ssd);
double utilization(struct ssd_info *ssd);

void data_migration_from_freezeblk_to_dram_for_WL(struct ssd_info * ssd);
void invalid_data_migration_from_freezeblk_to_dram_for_WL(struct  ssd_info *ssd);
void invalid_data_migration_from_freezeblk_to_dram(struct  ssd_info *ssd);
struct ssd_info * insert2dram_for_WL(struct ssd_info *ssd,unsigned int lpn,int state,struct sub_request *sub,struct request *req, int flag_form_req);

struct ssd_info *buffer_for_WL_management(struct ssd_info *ssd);
