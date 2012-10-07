/*
 * Copyright (c) 2012 by Paweł Lebioda <pawel.lebioda89@gmail.com>
 *
 * This file is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fat.h"

uint8_t	fat_open(struct fat_fs * fs,struct partition * partition)
{
	if(!fs || !partition)
		return 0;
	fs->partition = partition;
	if(fat_read_header(fs))
		return 0;
	return 1;
}
uint8_t	fat_close(struct fat_fs * fs)
{
	if(!fs)
		return 0;
	fs->partition = 0;
	return 1;
}
void print_fat_boot_sector(struct fat_boot_sector * fbs)
{
	DBG_INFO("JumpNOP: %x%x%x\n",fbs->JumpNOP[0],fbs->JumpNOP[1],fbs->JumpNOP[2]);
	DBG_INFO("OEM: %c%c%c%c%c%c%c%c\n",fbs->OEM[0],fbs->OEM[1],fbs->OEM[2],fbs->OEM[3],
					fbs->OEM[4],fbs->OEM[5],fbs->OEM[6],fbs->OEM[7]);
	DBG_INFO("Sector size: %d\n",fbs->SectorSize);
	DBG_INFO("Cluster size: %d\n",fbs->ClusterSize);
	DBG_INFO("Reserved sec: %d\n",fbs->ResSectors);
	DBG_INFO("Fat copies: %d\n",fbs->NumFATCop);
	DBG_INFO("Max root dir entry: %d\n",fbs->MaxRootDirEntr);
	DBG_INFO("Num sec < 32MB: %d\n",fbs->NumSec32MB);
	DBG_INFO("Media desc: %d\n",fbs->MediaDesc);
	DBG_INFO("Sec per FAT: %d\n",fbs->SecPerFAT);
	DBG_INFO("Sec per track: %d\n",fbs->SecPerTrack);
	DBG_INFO("Num of heads: %d\n",fbs->NumHeads);
	DBG_INFO("Hidden sec: %d\n",fbs->NumHiddenSec);
	DBG_INFO("Sectors: %d\n",fbs->NumSec);
	DBG_INFO("Logic Drive num: %d\n",fbs->LogicDriveNum);
	DBG_INFO("Extended sig(0x29): %02x\n",fbs->extendedSig);
	DBG_INFO("SN: %x\n",fbs->SN);
	DBG_INFO("Vol name: %c%c%c%c%c%c%c%c%c%c%c\n",fbs->VolName[0],fbs->VolName[1],
				fbs->VolName[2],fbs->VolName[3],fbs->VolName[4],fbs->VolName[5],
				fbs->VolName[6],fbs->VolName[7],fbs->VolName[8],fbs->VolName[9],
				fbs->VolName[10]);
	DBG_INFO("Fat name: %c%c%c%c%c%c%c%c\n",fbs->FATName[0],fbs->FATName[1],
				fbs->FATName[2],fbs->FATName[3],fbs->FATName[4],fbs->FATName[5],
				fbs->FATName[6],fbs->FATName[7]);
}

uint8_t	fat_read_header(struct fat_fs * fs)
{
	struct fat_boot_sector fatbs;
	struct partition * partition;
	struct fat_header * fh;
	if(!fs)
		return 0;
	partition = fs->partition;
	
	offset_t partition_offset = (offset_t)(partition->offset << 9);
	fh = &fs->header;
	
	partition->device_read(partition_offset,(uint8_t*)&fatbs,sizeof(fatbs));
	print_fat_boot_sector(&fatbs);
	uint32_t sector_count;
	if(fatbs.NumSec32MB == 0)
	{
		if(fatbs.NumSec == 0)
			return 0;
		sector_count = fatbs.NumSec;
	}
	else
	{
		sector_count = fatbs.NumSec32MB;
	}
	uint16_t sector_size = fatbs.SectorSize;
	uint16_t cluster_size = fatbs.ClusterSize;
	uint16_t reserved_sectors = fatbs.ResSectors;
	uint8_t fat_copies = fatbs.NumFATCop;
	uint16_t sectors_per_fat = fatbs.SecPerFAT;
	uint32_t max_root_entries_bytes = fatbs.MaxRootDirEntr;
	max_root_entries_bytes <<= 5;
	uint32_t data_sector_count = sector_count
				- reserved_sectors
				- (uint32_t) sectors_per_fat * fatbs.NumFATCop
				//integer number of sectors
				- ((max_root_entries_bytes + sector_size - 1) / sector_size );
	uint32_t data_cluster_count = data_sector_count / cluster_size;
	memset((uint8_t*)fh,sizeof(struct fat_header),0);
	fh->sector_size = sector_size;
	fh->size = (uint32_t) sector_size * sector_count;
	fh->cluster_size = (uint16_t) sector_size * cluster_size; 
	
	fh->fat_offset =	/* jump to partition */
				(offset_t) partition_offset 
				/* jump to fat */
				+ (offset_t) reserved_sectors * sector_size;
				
	/* the first 2 clusters are reserved*/
	fh->fat_size = ( data_cluster_count + 2 ) * sizeof(cluster_t);
	
	fh->sector_size = sector_size;
	fh->cluster_size = (uint16_t) sector_size * cluster_size;
	
	fh->root_dir_offset = fh->fat_offset 
		+(offset_t)fat_copies * sectors_per_fat * sector_size;
	fh->cluster_zero_offset = fh->root_dir_offset + (offset_t)max_root_entries_bytes;
	return 1;
}

cluster_t fat_get_next_cluster(struct fat_fs * fs,cluster_t cluster)
{
	if(!fs || cluster < 2)
		return 1;
	
	offset_t cluster_offset = fs->header.fat_offset + (offset_t)cluster*sizeof(cluster);
	cluster_t next_cluster;
	
	if(!fs->partition->device_read(cluster_offset,(uint8_t*)&next_cluster,sizeof(next_cluster)))
		return 1;
	
	
	if(next_cluster == FAT16_CLUSTER_FREE ||
		next_cluster == FAT16_CLUSTER_BAD || 
		(next_cluster >= FAT16_CLUSTER_RESERVED_MIN && next_cluster <= FAT16_CLUSTER_RESERVED_MAX) || 
		(next_cluster >= FAT16_CLUSTER_LAST_MIN && next_cluster <= FAT16_CLUSTER_LAST_MAX))
		return 1;
	
	return next_cluster;
}

offset_t fat_cluster_offset(struct fat_fs * fs,cluster_t cluster)
{
	if(!fs || cluster < 2)
		return 0;
	
	return ( fs->header.cluster_zero_offset 
	/* substract 2 becouse first two clusters are reserved and cluster_zero_offset points to 2 cluster*/
	+ (offset_t)(cluster-2)*fs->header.cluster_size);
}

uint8_t fat_get_dir_entry(struct fat_fs * fs,struct fat_dir_entry * dir_entry,const char * path)
{
	if(!fs || !path || path[0] == '\0' || !dir_entry)
		return 0;
//	 struct fat_dir dir;		
	uint8_t ret;
	if(path[0] == '/')
	{
		DBG_INFO("begin with the root directory\n");
		/* begin with the root directory */
		memset(dir_entry,0,sizeof(*dir_entry));
		ret = fat_read_dir_entry_from_offset(fs,dir_entry,fs->header.root_dir_offset,sizeof(struct fat_root_dir),0);
		dir_entry->first_cluster = 0;
		dir_entry->file_size = fs->header.cluster_zero_offset - fs->header.root_dir_offset;
		path++;
	}
	struct fat_dir dir;
	
	while(1)
	{
		DBG_INFO("Path = %s\n",path);
		if(path[0] == '\0')
			return 1;
		const char * sub_path = strchr(path,'/');
		DBG_INFO("subPath = %s\n",sub_path);
		uint8_t length_to_sep;
		if(sub_path)
		{
			DBG_INFO("Sub path != 0\n");
			length_to_sep = sub_path - path;
			++sub_path;
		}
		else
		{
			DBG_INFO("Sub path == 0\n");
			length_to_sep = strlen(path);
			sub_path = path + length_to_sep;
		}
		DBG_INFO("Opened dir %s\n",dir_entry->filename);
		DBG_INFO("Dir entry first_cluster: %x\n",dir_entry->first_cluster);
		DBG_INFO("Dir entry size: %x\n",dir_entry->file_size);
		DBG_INFO("Dir entry offset: %lx\n",dir_entry->entry_offset);
		DBG_INFO("Dir entry name: %s\n",dir_entry->filename);
		DBG_INFO("Dire entry attr = %x\n",dir_entry->attributes);
		if(!fat_dir_open(&dir,dir_entry))
			return 0;
		while(fat_read_dir(&dir,dir_entry))
		{
			DBG_INFO("File name = %s, length = %d\n",dir_entry->filename,strlen(dir_entry->filename));
			if(strlen(dir_entry->filename) != length_to_sep
				|| strncmp(path,dir_entry->filename,length_to_sep))
				continue;
			DBG_INFO("File found\n");			
			fat_dir_close(&dir);
			
			if(path[length_to_sep] == '\0')
			{
				DBG_INFO("Found name = %s\n",dir_entry->filename);				
				/* we parsed whole path and found the file */
				return 1;
			}
			
			if(dir_entry->attributes == FAT_ATTR_DIR)
			{
				DBG_INFO("we found parent dir\n");
				path = sub_path;
				break;
			}
			DBG_INFO("Return\n");
			return 0;
		}
		fat_dir_close(&dir);
		if(path != sub_path)
			break;
	}
	return 0;
}

uint8_t fat_dir_close(struct fat_dir * fat_dir)
{
	return 1;
}

uint8_t fat_dir_open(struct fat_dir * fat_dir,struct fat_dir_entry * dir_entry)
{
	DBG_INFO("fat_dir=%x dir_entry=%x,attr=%x\n",fat_dir,dir_entry,dir_entry->attributes);
	if(!fat_dir || !dir_entry || !(dir_entry->attributes == FAT_ATTR_DIR || dir_entry->attributes == FAT_ATTR_VOLUME))
		return 0;
	DBG_INFO("here\n");
	fat_dir->fs = dir_entry->fs;
	memcpy(&fat_dir->dir_entry,dir_entry,sizeof(*dir_entry));
	fat_dir->current_cluster = dir_entry->first_cluster;
	fat_dir->file_offset = 0;	 
	fat_dir->cluster_offset = 0;
	return 1;
}

uint8_t	fat_read_dir_entry_from_offset(struct fat_fs * fs,struct fat_dir_entry * dir_entry,offset_t offset,uint32_t length,uint8_t skip_volume)
{
	struct fat_root_dir root_dir;
	fs->partition->device_read(offset,(uint8_t*)&root_dir,length);
	/* skip deleted of empty entries*/
	if(root_dir.filename[0] == FAT_DIRENTRY_DELETED
		|| !root_dir.filename[0]
#if !FAT_LFN_SUPPORT		
		|| root_dir.attributes == FAT_ATTR_LFN
#endif		
		|| (skip_volume && root_dir.attributes == FAT_ATTR_VOLUME)
		)
		return 0;
	
	dir_entry->attributes = root_dir.attributes;
	dir_entry->first_cluster = root_dir.first_cluster;
	dir_entry->file_size = root_dir.file_size;
	dir_entry->entry_offset = offset;
	dir_entry->fs = fs;
	memset(&dir_entry->filename,0,sizeof(root_dir.filename));
	const char * space = strchr(root_dir.filename,' ');
	uint8_t name_len;
	if(space)
	{
		name_len = space - root_dir.filename;
	}
	else
	{
		name_len = strlen(root_dir.filename);
	}
	memcpy(&dir_entry->filename,root_dir.filename,name_len);

#if FAT_DATE_TIME_SUPPORT && FAT_DATE_TIME_SUPPORT_CREATION
	dir_entry->creation_time = root_dir.creation_time;
	dir_entry->creation_date = root_dir.creation_date;
#endif

#if FAT_DATE_TIME_SUPPORT && FAT_DATE_TIME_SUPPORT_MODIFICATION
	dir_entry->modification_time = root_dir.modification_time;
	dir_entry->modification_date = root_dir.modification_date;
#endif

#if FAT_DATE_TIME_SUPPORT && FAT_DATE_TIME_SUPPORT_LAST_ACCESS
	dir_entry->last_access_date = root_dir.last_access_date;
#endif	
	return 1;
}

uint8_t fat_read_dir(struct fat_dir * fat_dir,struct fat_dir_entry * dir_entry)
{
	if(!fat_dir || !dir_entry)
		return 0;
	uint8_t ret;
	struct fat_fs * fs = fat_dir->fs;
	struct fat_header * header = &fs->header;
//	 struct partition * partition = fs->partition;
	cluster_t cluster_num = fat_dir->current_cluster;
	uint32_t cluster_size;
	/* Root directory */
	if(cluster_num == 0)
		cluster_size = header->cluster_zero_offset-header->root_dir_offset;
	else
		cluster_size = header->cluster_size;	 
		uint32_t data_length = sizeof(struct fat_root_dir);
	uint8_t dir_entry_found = 0;
	while(!dir_entry_found)
	{
	// 	DBG_INFO("Dir %s size: %d\n",fat_dir->dir_entry.filename,fat_dir->dir_entry.file_size);
	// 	if(fat_dir->file_offset + data_length > fat_dir->dir_entry.file_size)
	// 	{
	// 		DBG_INFO("Return0\n");
	// 		return 0;
	// 	}
	// 	DBG_INFO("Cluster_offset: %lx\n",fat_dir->cluster_offset);
	// 	DBG_INFO("Cluster_size: %lx\n",cluster_size);
		if(fat_dir->cluster_offset + data_length > cluster_size)
		{
			DBG_ERROR("cluster: %x\n",cluster_num);
			cluster_num = fat_get_next_cluster(fs,cluster_num);
			DBG_ERROR("next cluster: %x\n",cluster_num);
		}
		if(cluster_num==1)
		{
			DBG_ERROR("!cluster_num: Return1\n");
			return 0;
		}
	
		offset_t read_offset = fat_dir->cluster_offset;
		if(cluster_num == 0)
			read_offset += fs->header.root_dir_offset; 
		else
			read_offset += fat_cluster_offset(fs,cluster_num);

	
		fat_dir->file_offset += data_length;
		fat_dir->cluster_offset += data_length;
		fat_dir->current_cluster = cluster_num;	
	
		DBG_INFO("Read offset %lx\n",read_offset);
		if((ret=fat_read_dir_entry_from_offset(fs,dir_entry,read_offset,data_length,1)))
			dir_entry_found = 1;
	// 	DBG_INFO("fat_read_root_dir ret = %d\n",ret);
	}
	DBG_INFO("Return2\n");
	return 1;
}

#if FAT_DATE_TIME_SUPPORT
uint16_t fat_get_year(uint16_t fat_date)
{
	/* 1980 + most significant 7 bits*/
	return (1980 + ((fat_date>>9)&0x7f));
}
uint8_t	fat_get_month(uint16_t fat_date)
{
	/*4 bits from 5 to 8*/
	return ((uint8_t)((fat_date>>5)&0xf));
}
uint8_t fat_get_day(uint16_t fat_date)
{
	/*least significant 5 bits*/
	return ((uint8_t)(fat_date&0x1f));
}
uint8_t fat_get_hours(uint16_t fat_time)
{
	return ((uint8_t)((fat_time>>11)&0x1f));
}
uint8_t	fat_get_minutes(uint16_t fat_time)
{
	return ((uint8_t)((fat_time>>5)&0x3f));	
}
uint8_t fat_get_seconds(uint16_t fat_time)
{
	return ((uint8_t)(fat_time&0x1f)<<1);	
}
#endif
