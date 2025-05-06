#pragma once

#include "rtklib.h"
#include <mutex>

struct artk_t
{
	rtcm_t* rtcm_obs;	/* rtcm decoder to base stream */
	rtcm_t* rtcm_nav;	/* rtcm decoder to brdc stream */
	obsd_t* obs;		/* for rover and base */
	double* xyz;		/* position (xyz) for rove and base */
	rtk_t* rtk;			/* rtk */
	prcopt_t* opt;		/* options */
	std::mutex obs_lock;/* lock for data input */
	std::mutex nav_lock;/* lock for brdc data */
	std::mutex rtk_lock;/* lock for RTK engine */
	artk_t();
	~artk_t();
	int add_rove_obs(obsd_t* obsd, int n, double* pos);
	int add_base_obs(obsd_t* obsd, int n, double* pos);
	int add_base_buf(char* buff, int nlen);
	int add_brdc(nav_t* nav);
	int add_brdc(char* buff, int nlen);
	int proc(char* gga, char *sol);
};
