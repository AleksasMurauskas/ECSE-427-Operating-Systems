void mksfs(int fresh);
int sfs_get_next_filename(char *fname);
int sfs_GetFileSize( char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fwseek(int fileID, int loc);
int sfs_frseek(int fileID, int loc);
int sfs_remove(char *file);

