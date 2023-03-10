#include "analyzer.h"

off_t fsize(const char *filename) {
    struct stat st; 

    if (stat(filename, &st) == 0)
        return st.st_size;

    return -1; 
}
const char *out_directory = OUT_DIRECTORY;

void* analyze(void* info)
{
    struct task_details* current_task = (struct task_details*) info;
    syslog(LOG_NOTICE,"Starting analyzing job from  %s\n", current_task->path);

    long long * sum_size = (long long *) malloc(sizeof(long long *));
    *sum_size = 0; 
    long long total_size = dfs_find_size_on_disk(current_task->path,current_task->task_id); // Need this to estimate progress
    syslog(LOG_NOTICE,"Total size of %s task id %d is %lld\n", current_task->path,current_task->task_id,total_size);

    char output_path[MAX_PATH_LENGTH];
    snprintf(output_path, MAX_PATH_LENGTH, "/tmp/da_daemon/%d", current_task->task_id);

    struct stat st = {0};

    if (stat(out_directory, &st) == -1) {
        mkdir(out_directory, 0777);
    }

    remove(output_path);
    FILE * out_fd = fopen(output_path, "w");
    syslog(LOG_NOTICE,"Output path is %s",output_path);

    write_report(current_task->path,"/",out_fd,total_size,0,current_task->task_id, sum_size);

    fclose(out_fd);

    notify_task_done(current_task->task_id);
    
    return NULL;
}

long long write_report(const char *path, const char* relative_path, FILE * out_fd, long long total_size, int depth, int task_id, long long * sum_size)
{
    permission_to_continue(task_id);
    DIR *dir = opendir(path);
    if(dir == NULL) 
    {
        return 0; // we are not in a directory
    }
    char sub_path[MAX_PATH_LENGTH] = "";
    char sub_relative_path[MAX_PATH_LENGTH] = "";

    struct dirent *sub_dir;
    long long size = 0;
    while(1) // for every file in current folder
    {
        sub_dir = readdir(dir);

        if(!sub_dir)break;

        if(sub_dir->d_type == DT_REG)
        {
            set_task_files_no(task_id, get_task_files_no(task_id) + 1);
        }
            
        if (strcmp(sub_dir->d_name, ".") != 0 && strcmp(sub_dir->d_name, "..") != 0) {

            add_to_path(path,sub_dir->d_name,sub_path);
            add_to_path(relative_path,sub_dir->d_name,sub_relative_path);

            // if(sub_dir->d_type != 4)// just in case we want to consider only containing files not folder size (4096)
            long long aux = (long long)fsize(sub_path);
             
            size += aux;
            *sum_size += aux;
            
            aux = write_report(sub_path,sub_relative_path,out_fd,total_size,depth + 1,task_id, sum_size);
            
            size += aux;
        }
    }
    closedir(dir);


    set_task_dirs_no(task_id, get_task_dirs_no(task_id) + 1);

    long long actual_size = size;

    if(depth != 0){
        actual_size += 4096;
    }
    double percent = (((long double) actual_size) / ((long double) total_size)) * 100;
    double print_size = ((long double) actual_size) / 1024;

    double max_hashtag = (double) MAX_HASHTAG;
    int curent_hashtag = (int) (max_hashtag * percent / 100);
    char hashtag[MAX_HASHTAG + 1] = "";
    for(int i = 0; i < curent_hashtag; i++)
        hashtag[i] = '#';

    if(depth == 0)
    {
        fprintf(out_fd, "%s  %0.2lf%%  %0.2lfKB  %s\n", path, percent, print_size, hashtag);
        fprintf(out_fd, "Path\tUsage\tSize\tAmount\n");
    }
    else if(depth == 1)
    {
        fprintf(out_fd, "|-%s  %0.2lf%%  %0.2lfKB  %s\n", relative_path, percent, print_size, hashtag);
        fprintf(out_fd, "|\n");
    }
    else
    {
        fprintf(out_fd, "|-%s  %0.2lf%%  %0.2lfKB  %s\n", relative_path, percent, print_size, hashtag);
    }
    set_task_progress(task_id, (double)((((double) *sum_size) / ((double) total_size)) * 100));

    // size is the size of this subdir
    return size;
}

long long dfs_find_size_on_disk(const char *path,int task_id)
{
    permission_to_continue(task_id);
    // syslog(LOG_NOTICE,"DFS in %s\n", path);
    DIR *dir = opendir(path);
    if(dir == NULL) 
    {
        return 0; // we are not in a directory
    }
    char sub_path[MAX_PATH_LENGTH] = "";

    struct dirent *sub_dir;
    long long size = 0;
    while(1) // for every file in current folder
    {
        sub_dir = readdir(dir);
        if(!sub_dir)break;
        if (strcmp(sub_dir->d_name, ".") != 0 && strcmp(sub_dir->d_name, "..") != 0) {
            add_to_path(path,sub_dir->d_name,sub_path);
            // if(sub_dir->d_type != 4)// just in case we want to consider only containing files
            size += (long long)fsize(sub_path);
            size += dfs_find_size_on_disk(sub_path,task_id);
        }
    }

    closedir(dir);
    return size;
}
