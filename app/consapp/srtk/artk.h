#pragma once

#include "rtklib.h"
#include <mutex>

typedef struct
{
	int    sat;				/* satellite system GPS(0),GLO(1),GAL(2),BD2(3),BD3(4),QZS(5),IRN(6) */
	int    sys;
	int    prn;
	double rs[6];
	double dts[2];
	double var;
	int    svh;
	double azel[2];
	double e[3];
	double r;
	double tro;
	double ion;
}svec_t;

int comp_vec(obsd_t* obs, svec_t* vec, int n, nav_t* nav);

/* rcv=1 rove, rcv=2 base, pos rove, pos+3 base */
int rtk_proc(obsd_t* obs, svec_t* vec, int n, double* pos, char *gga);

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
