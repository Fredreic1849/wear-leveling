#include "avlTree.h"
#include "flash.h"
#include "initialize.h"
#include "pagemap.h"
#include "ssd.h"
#include "WL.h"

#define WL_threshold 5000

int total_erase_count(struct ssd_info *ssd){
    int m;
    int erase = 0;
    for(m=0;m<ssd->parameter->block_plane;m++)
    {
        if(ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[m].erase_count>0)
        {
            erase=erase+ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[m].erase_count;
        }
    }
    return erase;
}

double single_block_utilization(struct ssd_info *ssd, int block_id){
   int i, j;
   int valid_page = 0;
   double utilization = 0;
   for(i = 0; i < ssd->parameter->page_block; i++){
       if (ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[block_id].page_head[i].valid_state > 0) {
           valid_page++;
       }
   }
    utilization = (double)(valid_page)/(double)(ssd->parameter->page_block);
   return utilization;
}

double utilization(struct ssd_info *ssd){
    int i, j;
    int isfrozen;
    int valid_page=0;
    int invalid_page_num = 0,free_page_num = 0,block_page_num = 0;
    double utilization;
    for(j = 0; j < ssd->parameter->block_plane; j++){
        for(i = 0; i < ssd->parameter->page_block; i++) {
            if (ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].valid_state > 0) {
                valid_page++;
            }
        }
//        free_page_num += ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].free_page_num;
    }
    block_page_num = ssd->parameter->page_block *  ssd->parameter->block_plane;
    free_page_num = block_page_num - invalid_page_num - valid_page;
    if(block_page_num-invalid_page_num-free_page_num == 0)
    {
        utilization = 0.0;
    }
    else
    {
        utilization = (double)(valid_page)/(double)block_page_num;
    }
//    printf("invalid pages:%d\nfree_pages:%d\nblock_pages:%d\n", invalid_page_num, free_page_num, block_page_num);

    printf("%d\n%lf\n",valid_page, utilization);
    return utilization;
}


int check_variance(double var){
    if(var >= WL_threshold ){
        return 1;
    }
    return 0;
}

double calculate_mean(struct ssd_info *ssd){
    int i;
    int sum_erasenum = 0;
    for(i = 0; i < ssd->parameter->block_plane; i++){
        sum_erasenum += ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[i].erase_count;
    }
    return (double)(sum_erasenum/ssd->parameter->block_plane);
}

double calculate_variance(struct ssd_info *ssd){
    int i;
    double mean = calculate_mean(ssd);
    double sum = 0, erase_count;
    for(i = 0; i <ssd->parameter->block_plane; i++){
        erase_count = (double)ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[i].erase_count;
        sum += (erase_count - mean)*(erase_count - mean);
    }
    return sum;
}

void freeze_highest_erase_count_blocks(struct ssd_info * ssd, int n){
    int i, j;
    int isfrozen, erase_count;
    int highest_erase_count = -1, highest_index = -1;
    for(i = 0; i < n; i++){
        for(j = 0; j < ssd->parameter->block_plane; j++){
            isfrozen = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].isfrozen;
            erase_count = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].erase_count;
            if(isfrozen != 1 && erase_count > highest_erase_count){
                highest_index = j;
                highest_erase_count = erase_count;
            }
        }
        ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[highest_index].isfrozen = 1;
        highest_erase_count = -1;
    }
}

void unfreeze_highest_erase_count_blocks(struct ssd_info * ssd){
    int j;
    int isfrozen;
    for(j = 0; j < ssd->parameter->block_plane; j++){
        isfrozen = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].isfrozen;
        if(isfrozen == 1){
            ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].isfrozen = 0;
        }
    }
}

void data_migration_from_freezeblk_to_dram_for_WL(struct ssd_info * ssd){
    //直接将冻结块的数据插入到dram_for_WL
    int i, j;
    int isfrozen;
    unsigned int lpn;
    unsigned int mask;
    int state = 15;
    unsigned int offset1 = 0;
    for(j = 0; j < ssd->parameter->block_plane; j++){
        isfrozen = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].isfrozen;
        if(isfrozen == 1){
            for(i = 0; i< ssd->parameter->page_block; i++){
                if(ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].valid_state > 0
                &&ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].free_state >0){
                    lpn = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].lpn;

                    state = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].free_state;
                    mask=~(0xffffffff<<(ssd->parameter->subpage_page));
                    state=mask;
                    state=state&(0xffffffff<<offset1);
                    ssd = insert2dram_for_WL(ssd, lpn, state, NULL, NULL, 1);
                }
            }
        }
    }

}

void invalid_data_migration_from_freezeblk_to_dram_for_WL(struct  ssd_info *ssd){
    int i, j, k;
    int isfrozen;
    unsigned int lpn;
    unsigned int state = 15;
    for(j = 0; j < ssd->parameter->block_plane; j++){
        isfrozen = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].isfrozen;
        if(isfrozen == 1){
            for(i = 0; i< ssd->parameter->page_block; i++){
                if(ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].valid_state == 0
                && ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].lpn != -1){ //为无效页的情况
                    lpn = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].lpn;
                    ssd = insert2dram_for_WL(ssd, lpn, state, NULL, NULL, 1);
                }
            }
        }
    }
}

void invalid_data_migration_from_freezeblk_to_dram(struct  ssd_info *ssd){
    int i, j;
    int isfrozen;
    unsigned int lpn;
    unsigned int state = 15;
    for(j = 0; j < ssd->parameter->block_plane; j++){
        isfrozen = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].isfrozen;
        if(isfrozen == 1){
            for(i = 0; i< ssd->parameter->page_block; i++){
                if(ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].valid_state == 0
                   && ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].lpn != -1){ //为无效页的情况

                    lpn = ssd->channel_head[0].chip_head[0].die_head[0].plane_head[0].blk_head[j].page_head[i].lpn;
                    ssd = insert2buffer(ssd, lpn, state, NULL, NULL);
                }
            }
        }
    }
}

struct ssd_info * insert2dram_for_WL(struct ssd_info *ssd,unsigned int lpn,int state,struct sub_request *sub,struct request *req, int flag_form_req)
{
    int write_back_count,flag=0;                                                             /*flag表示为写入新数据腾空间是否完成，0表示需要进一步腾，1表示已经腾空*/
    unsigned int i,lsn,hit_flag,add_flag,sector_count,active_region_flag=0,free_sector=0;
    struct buffer_group *buffer_node=NULL,*pt,*new_node=NULL,key;
    struct sub_request *sub_req=NULL,*update=NULL;


    unsigned int sub_req_state=0, sub_req_size=0,sub_req_lpn=0;

    sector_count=size(state);                                                                /*需要写到buffer的sector个数*/
    key.group=lpn;

    //若flag为1，则是由req引起的，且buffer_node已经在dram_for_WL中

    buffer_node= (struct buffer_group*)avlTreeFind(ssd->dram_for_WL->buffer, (TREE_NODE *)&key);    /*在平衡二叉树中寻找buffer node*/

    //若dram_for_WL中没找到，则在dram中找
    if(buffer_node == NULL && flag_form_req == 0){
        buffer_node = (struct buffer_group*)avlTreeFind(ssd->dram->buffer, (TREE_NODE *)&key);
    }

    /************************************************************************************************
     *没有命中。
     *第一步根据这个lpn有多少子页需要写到buffer，去除已写回的lsn，为该lpn腾出位置，
     *首先即要计算出free sector（表示还有多少可以直接写的buffer节点）。
     *如果free_sector>=sector_count，即有多余的空间够lpn子请求写，不需要产生写回请求
     *否则，没有多余的空间供lpn子请求写，这时需要释放一部分空间，产生写回请求。就要creat_sub_request()
     *************************************************************************************************/
    if(buffer_node==NULL)
    {
        free_sector = ssd->dram_for_WL->buffer->max_buffer_sector - ssd->dram_for_WL->buffer->buffer_sector_count;
        if(free_sector>=sector_count)
        {
            flag=1;
        }
        if(flag==0)
        {
            write_back_count=sector_count-free_sector;
            ssd->dram_for_WL->buffer->write_miss_hit=ssd->dram_for_WL->buffer->write_miss_hit+ write_back_count;
            while(write_back_count>0)
            {
                sub_req = NULL;
                sub_req_state = ssd->dram_for_WL->buffer->buffer_tail->stored;
                sub_req_size = size(ssd->dram_for_WL->buffer->buffer_tail->stored);
                sub_req_lpn = ssd->dram_for_WL->buffer->buffer_tail->group;
                sub_req = creat_sub_request(ssd,sub_req_lpn,sub_req_size,sub_req_state,req,WRITE);

                /**********************************************************************************
                 *req不为空，表示这个insert2buffer函数是在buffer_management中调用，传递了request进来
                 *req为空，表示这个函数是在process函数中处理一对多映射关系的读的时候，需要将这个读出
                 *的数据加到buffer中，这可能产生实时的写回操作，需要将这个实时的写回操作的子请求挂在
                 *这个读请求的总请求上
                 ***********************************************************************************/
                if(req!=NULL)
                {
                }
                else if(sub != NULL && req == NULL)
                {
                    sub_req->next_subs=sub->next_subs;
                    sub->next_subs=sub_req;
                }

                /*********************************************************************
                 *写请求插入到了平衡二叉树，这时就要修改dram的buffer_sector_count；
                 *维持平衡二叉树调用avlTreeDel()和AVL_TREENODE_FREE()函数；维持LRU算法；
                 **********************************************************************/
                ssd->dram_for_WL->buffer->buffer_sector_count=ssd->dram_for_WL->buffer->buffer_sector_count-sub_req->size;
                pt = ssd->dram_for_WL->buffer->buffer_tail;
                avlTreeDel(ssd->dram_for_WL->buffer, (TREE_NODE *) pt);
                if(ssd->dram_for_WL->buffer->buffer_head->LRU_link_next == NULL){
                    ssd->dram_for_WL->buffer->buffer_head = NULL;
                    ssd->dram_for_WL->buffer->buffer_tail = NULL;
                }else{
                    ssd->dram_for_WL->buffer->buffer_tail=ssd->dram_for_WL->buffer->buffer_tail->LRU_link_pre;
                    ssd->dram_for_WL->buffer->buffer_tail->LRU_link_next=NULL;
                }
                pt->LRU_link_next=NULL;
                pt->LRU_link_pre=NULL;
                AVL_TREENODE_FREE(ssd->dram_for_WL->buffer, (TREE_NODE *) pt);
                pt = NULL;

                write_back_count = write_back_count - sub_req->size;                            /*因为产生了实时写回操作，需要将主动写回操作区域增加*/
            }
        }

        /******************************************************************************
         *生成一个buffer node，根据这个页的情况分别赋值个各个成员，添加到队首和二叉树中
         *******************************************************************************/
        new_node=NULL;
        new_node=(struct buffer_group *)malloc(sizeof(struct buffer_group));
        alloc_assert(new_node,"buffer_group_node");
        memset(new_node,0, sizeof(struct buffer_group));

        new_node->group=lpn;
        new_node->stored=state;
        new_node->dirty_clean=state;
        new_node->LRU_link_pre = NULL;
        new_node->LRU_link_next=ssd->dram_for_WL->buffer->buffer_head;
        if(ssd->dram_for_WL->buffer->buffer_head != NULL){
            ssd->dram_for_WL->buffer->buffer_head->LRU_link_pre=new_node;
        }else{
            ssd->dram_for_WL->buffer->buffer_tail = new_node;
        }
        ssd->dram_for_WL->buffer->buffer_head=new_node;
        new_node->LRU_link_pre=NULL;
        avlTreeAdd(ssd->dram_for_WL->buffer, (TREE_NODE *) new_node);
        ssd->dram_for_WL->buffer->buffer_sector_count += sector_count;
    }
        /****************************************************************************************
         *在buffer中命中的情况
         *算然命中了，但是命中的只是lpn，有可能新来的写请求，只是需要写lpn这一page的某几个sub_page
         *这时有需要进一步的判断
         *****************************************************************************************/
    else
    {
        for(i=0;i<ssd->parameter->subpage_page;i++)
        {
            /*************************************************************
             *判断state第i位是不是1
             *并且判断第i个sector是否存在buffer中，1表示存在，0表示不存在。
             **************************************************************/
            if((state>>i)%2!=0)
            {
                lsn=lpn*ssd->parameter->subpage_page+i;
                hit_flag=0;
                hit_flag=(buffer_node->stored)&(0x00000001<<i);

                if(hit_flag!=0)				                                          /*命中了，需要将该节点移到buffer的队首，并且将命中的lsn进行标记*/
                {
                    active_region_flag=1;                                             /*用来记录在这个buffer node中的lsn是否被命中，用于后面对阈值的判定*/

                    if(req!=NULL)
                    {
                        if(ssd->dram_for_WL->buffer->buffer_head!=buffer_node)
                        {
                            if(ssd->dram_for_WL->buffer->buffer_tail==buffer_node)
                            {
                                ssd->dram_for_WL->buffer->buffer_tail=buffer_node->LRU_link_pre;
                                buffer_node->LRU_link_pre->LRU_link_next=NULL;
                            }
                            else if(buffer_node != ssd->dram_for_WL->buffer->buffer_head)
                            {
                                buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;
                                buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;
                            }
                            buffer_node->LRU_link_next=ssd->dram_for_WL->buffer->buffer_head;
                            ssd->dram_for_WL->buffer->buffer_head->LRU_link_pre=buffer_node;
                            buffer_node->LRU_link_pre=NULL;
                            ssd->dram_for_WL->buffer->buffer_head=buffer_node;
                        }
                        ssd->dram_for_WL->buffer->write_hit++;
                        req->complete_lsn_count++;                                        /*关键 当在buffer中命中时 就用req->complete_lsn_count++表示往buffer中写了数据。*/
                    }
                    else
                    {
                    }
                }
                else
                {
                    /************************************************************************************************************
                     *该lsn没有命中，但是节点在buffer中，需要将这个lsn加到buffer的对应节点中
                     *从buffer的末端找一个节点，将一个已经写回的lsn从节点中删除(如果找到的话)，更改这个节点的状态，同时将这个新的
                     *lsn加到相应的buffer节点中，该节点可能在buffer头，不在的话，将其移到头部。如果没有找到已经写回的lsn，在buffer
                     *节点找一个group整体写回，将这个子请求挂在这个请求上。可以提前挂在一个channel上。
                     *第一步:将buffer队尾的已经写回的节点删除一个，为新的lsn腾出空间，这里需要修改队尾某节点的stored状态这里还需要
                     *       增加，当没有可以之间删除的lsn时，需要产生新的写子请求，写回LRU最后的节点。
                     *第二步:将新的lsn加到所述的buffer节点中。
                     *************************************************************************************************************/
                    ssd->dram_for_WL->buffer->write_miss_hit++;

                    if(ssd->dram_for_WL->buffer->buffer_sector_count>=ssd->dram_for_WL->buffer->max_buffer_sector)
                    {
                        if (buffer_node==ssd->dram_for_WL->buffer->buffer_tail)                  /*如果命中的节点是buffer中最后一个节点，交换最后两个节点*/
                        {
                            pt = ssd->dram_for_WL->buffer->buffer_tail->LRU_link_pre;
                            ssd->dram_for_WL->buffer->buffer_tail->LRU_link_pre=pt->LRU_link_pre;
                            ssd->dram_for_WL->buffer->buffer_tail->LRU_link_pre->LRU_link_next=ssd->dram_for_WL->buffer->buffer_tail;
                            ssd->dram_for_WL->buffer->buffer_tail->LRU_link_next=pt;
                            pt->LRU_link_next=NULL;
                            pt->LRU_link_pre=ssd->dram_for_WL->buffer->buffer_tail;
                            ssd->dram_for_WL->buffer->buffer_tail=pt;

                        }
                        sub_req=NULL;
                        sub_req_state=ssd->dram_for_WL->buffer->buffer_tail->stored;
                        sub_req_size=size(ssd->dram_for_WL->buffer->buffer_tail->stored);
                        sub_req_lpn=ssd->dram_for_WL->buffer->buffer_tail->group;
                        sub_req=creat_sub_request(ssd,sub_req_lpn,sub_req_size,sub_req_state,req,WRITE);

                        if(req!=NULL)
                        {

                        }
                        else if(req==NULL)
                        {
                            sub_req->next_subs=sub->next_subs;
                            sub->next_subs=sub_req;
                        }

                        ssd->dram_for_WL->buffer->buffer_sector_count=ssd->dram_for_WL->buffer->buffer_sector_count-sub_req->size;
                        pt = ssd->dram_for_WL->buffer->buffer_tail;
                        avlTreeDel(ssd->dram_for_WL->buffer, (TREE_NODE *) pt);

                        /************************************************************************/
                        /* 改:  挂在了子请求，buffer的节点不应立即删除，						*/
                        /*			需等到写回了之后才能删除									*/
                        /************************************************************************/
                        if(ssd->dram_for_WL->buffer->buffer_head->LRU_link_next == NULL)
                        {
                            ssd->dram_for_WL->buffer->buffer_head = NULL;
                            ssd->dram_for_WL->buffer->buffer_tail = NULL;
                        }else{
                            ssd->dram_for_WL->buffer->buffer_tail=ssd->dram_for_WL->buffer->buffer_tail->LRU_link_pre;
                            ssd->dram_for_WL->buffer->buffer_tail->LRU_link_next=NULL;
                        }
                        pt->LRU_link_next=NULL;
                        pt->LRU_link_pre=NULL;
                        AVL_TREENODE_FREE(ssd->dram_for_WL->buffer, (TREE_NODE *) pt);
                        pt = NULL;
                    }

                    /*第二步:将新的lsn加到所述的buffer节点中*/
                    add_flag=0x00000001<<(lsn%ssd->parameter->subpage_page);

                    if(ssd->dram_for_WL->buffer->buffer_head!=buffer_node)                      /*如果该buffer节点不在buffer的队首，需要将这个节点提到队首*/
                    {
                        if(ssd->dram_for_WL->buffer->buffer_tail==buffer_node)
                        {
                            buffer_node->LRU_link_pre->LRU_link_next=NULL;
                            ssd->dram_for_WL->buffer->buffer_tail=buffer_node->LRU_link_pre;
                        }
                        else
                        {
                            buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;
                            buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;
                        }
                        buffer_node->LRU_link_next=ssd->dram_for_WL->buffer->buffer_head;
                        ssd->dram_for_WL->buffer->buffer_head->LRU_link_pre=buffer_node;
                        buffer_node->LRU_link_pre=NULL;
                        ssd->dram_for_WL->buffer->buffer_head=buffer_node;
                    }
                    buffer_node->stored=buffer_node->stored|add_flag;
                    buffer_node->dirty_clean=buffer_node->dirty_clean|add_flag;
                    ssd->dram_for_WL->buffer->buffer_sector_count++;
                }

            }
        }
    }

    return ssd;
}

struct ssd_info *buffer_for_WL_management(struct ssd_info *ssd)
{
    unsigned int j,lsn,lpn,last_lpn,first_lpn,index,complete_flag=0, state,full_page;
    unsigned int flag=0,need_distb_flag,lsn_flag,flag1=1,active_region_flag=0;
    struct request *new_request;
    struct buffer_group *buffer_node,key;
    unsigned int mask=0,offset1=0,offset2=0;

    ssd->dram_for_WL->current_time=ssd->current_time;
    full_page=~(0xffffffff<<ssd->parameter->subpage_page);

    new_request=ssd->request_tail;
    lsn=new_request->lsn;
    lpn=new_request->lsn/ssd->parameter->subpage_page;
    last_lpn=(new_request->lsn+new_request->size-1)/ssd->parameter->subpage_page;
    first_lpn=new_request->lsn/ssd->parameter->subpage_page;

    new_request->need_distr_flag=(unsigned int*)malloc(sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));
    alloc_assert(new_request->need_distr_flag,"new_request->need_distr_flag");
    memset(new_request->need_distr_flag, 0, sizeof(unsigned int)*((last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32+1));

    if(new_request->operation==READ)
    {
        while(lpn<=last_lpn)
        {
            /************************************************************************************************
             *need_distb_flag表示是否需要执行distribution函数，1表示需要执行，buffer中没有，0表示不需要执行
             *即1表示需要分发，0表示不需要分发，对应点初始全部赋为1
             *************************************************************************************************/
            need_distb_flag=full_page;
            key.group=lpn;
            buffer_node= (struct buffer_group*)avlTreeFind(ssd->dram_for_WL->buffer, (TREE_NODE *)&key);		// buffer node

            while((buffer_node!=NULL)&&(lsn<(lpn+1)*ssd->parameter->subpage_page)&&(lsn<=(new_request->lsn+new_request->size-1)))
            {
                lsn_flag=full_page;
                mask=1 << (lsn%ssd->parameter->subpage_page);
                if(mask>31)
                {
                    printf("the subpage number is larger than 32!add some cases");
                    getchar();
                }
                else if((buffer_node->stored & mask)==mask)
                {
                    flag=1;
                    lsn_flag=lsn_flag&(~mask);
                }

                if(flag==1)
                {	//如果该buffer节点不在buffer的队首，需要将这个节点提到队首，实现了LRU算法，这个是一个双向队列。
                    if(ssd->dram_for_WL->buffer->buffer_head!=buffer_node)
                    {
                        if(ssd->dram_for_WL->buffer->buffer_tail==buffer_node)
                        {
                            buffer_node->LRU_link_pre->LRU_link_next=NULL;
                            ssd->dram_for_WL->buffer->buffer_tail=buffer_node->LRU_link_pre;
                        }
                        else
                        {
                            buffer_node->LRU_link_pre->LRU_link_next=buffer_node->LRU_link_next;
                            buffer_node->LRU_link_next->LRU_link_pre=buffer_node->LRU_link_pre;
                        }
                        buffer_node->LRU_link_next=ssd->dram->buffer->buffer_head;
                        ssd->dram_for_WL->buffer->buffer_head->LRU_link_pre=buffer_node;
                        buffer_node->LRU_link_pre=NULL;
                        ssd->dram_for_WL->buffer->buffer_head=buffer_node;
                    }
                    ssd->dram_for_WL->buffer->read_hit++;
                    new_request->complete_lsn_count++;
                }
                else if(flag==0)
                {
                    ssd->dram_for_WL->buffer->read_miss_hit++;
                }

                need_distb_flag=need_distb_flag&lsn_flag;

                flag=0;
                lsn++;
            }

            index=(lpn-first_lpn)/(32/ssd->parameter->subpage_page);
            new_request->need_distr_flag[index]=new_request->need_distr_flag[index]|(need_distb_flag<<(((lpn-first_lpn)%(32/ssd->parameter->subpage_page))*ssd->parameter->subpage_page));
            lpn++;

        }
    }
    else if(new_request->operation==WRITE)
    {
        while(lpn<=last_lpn)
        {
            need_distb_flag=full_page;
            mask=~(0xffffffff<<(ssd->parameter->subpage_page));
            state=mask;

            if(lpn==first_lpn)
            {
                offset1=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-new_request->lsn);
                state=state&(0xffffffff<<offset1);
            }
            if(lpn==last_lpn)
            {
                offset2=ssd->parameter->subpage_page-((lpn+1)*ssd->parameter->subpage_page-(new_request->lsn+new_request->size));
                state=state&(~(0xffffffff<<offset2));
            }

            ssd= insert2dram_for_WL(ssd, lpn, state, NULL, new_request, 0);
            lpn++;
        }
    }
    complete_flag = 1;
    for(j=0;j<=(last_lpn-first_lpn+1)*ssd->parameter->subpage_page/32;j++)
    {
        if(new_request->need_distr_flag[j] != 0)
        {
            complete_flag = 0;
        }
    }

    /*************************************************************
     *如果请求已经被全部由buffer服务，该请求可以被直接响应，输出结果
     *这里假设dram的服务时间为1000ns
     **************************************************************/
    if((complete_flag == 1)&&(new_request->subs==NULL))
    {
        new_request->begin_time=ssd->current_time;
        new_request->response_time=ssd->current_time+1000;
    }


    return ssd;
}



