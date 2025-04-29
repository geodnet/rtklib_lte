#include "srtk.h"


artk_t::artk_t()
{
	rtcm_obs = new rtcm_t;
	rtcm_nav = new rtcm_t;
	xyz = new double[6];
	obs = new obsd_t[MAXOBS + MAXOBS];
	rtk = new rtk_t;
	opt = new prcopt_t;
	memset(rtcm_obs, 0, sizeof(rtcm_t));
	memset(rtcm_nav, 0, sizeof(rtcm_t));
	memset(xyz, 0, sizeof(double) * 6);
	memset(rtk, 0, sizeof(rtk_t));
	memset(opt, 0, sizeof(prcopt_t));
	init_rtcm(rtcm_obs);
	init_rtcm(rtcm_nav);
	*opt = prcopt_default;
	opt->mode = PMODE_KINEMA;
	opt->navsys = SYS_GPS | SYS_GAL | SYS_CMP;
	opt->elmin = 10.0 * D2R;
	opt->refpos = POSOPT_RTCM;
	opt->glomodear = 0;/* GLO AR OFF */
	opt->bdsmodear = 0;/* BDS AR OFF*/
	opt->dynamics = 0; /* use PVA model */
	opt->modear = 0;	/* AR OFF */
	opt->nf = 2;/* default dual band L1+L2 */
	opt->ionoopt = IONOOPT_BRDC;// IONOOPT_IFLC;
	opt->tropopt = TROPOPT_SAAS;// TROPOPT_EST;
	//opt->ionoopt = IONOOPT_IFLC;
	//opt->tropopt = TROPOPT_EST;
	rtkinit(rtk, opt);
}
artk_t::~artk_t()
{
	free_rtcm(rtcm_obs);
	free_rtcm(rtcm_nav);
	delete rtcm_obs;
	delete rtcm_nav;
	delete[] xyz;
	delete[] obs;
	delete rtk;
	delete opt;
}
int artk_t::add_rove_obs(obsd_t* obsd, int n, double* pos)
{
	int ret = 0;
	int i = 0;
	if (obs_lock.try_lock())
	{
		double* xyz_rov = xyz + 0;
		if (n > 0)
		{
			memset(obs, 0, sizeof(obsd_t) * MAXOBS);
			for (i = 0; i < n; ++i)
			{
				obs[i] = obsd[i];
				obs[i].rcv = 1;
			}
			xyz_rov[0] = pos[0];
			xyz_rov[1] = pos[1];
			xyz_rov[2] = pos[2];
		}
		obs_lock.unlock();
		ret = 1;
	}
	return ret;
}
int artk_t::add_base_obs(obsd_t* obsd, int n, double* pos)
{
	int ret = 0;
	int i = 0;
	if (obs_lock.try_lock())
	{
		double* xyz_bas = xyz + 3;
		memset(obs + MAXOBS, 0, sizeof(obsd_t) * MAXOBS);
		for (i = 0; i < n; ++i)
		{
			obs[MAXOBS + i] = obsd[i];
			obs[MAXOBS + i].rcv = 2;
		}
		xyz_bas[0] = pos[0];
		xyz_bas[1] = pos[1];
		xyz_bas[2] = pos[2];
		obs_lock.unlock();
		ret = 1;
	}
	return ret;
}
int artk_t::add_base_buf(char* buff, int nlen)
{
	int ret = 0;
	int i = 0;
	int j = 0;
	int sat = 0;
	int sys = 0;
	int prn = 0;
	if (obs_lock.try_lock())
	{
		double* xyz_bas = xyz + 3;
		for (i = 0; i < nlen; ++i)
		{
			ret = input_rtcm3(rtcm_obs, (uint8_t)buff[i]);
			if (ret == 1)
			{
				for (j = 0; j < rtcm_obs->obs.n; ++j)
				{
					obs[MAXOBS + j] = rtcm_obs->obs.data[j];
					obs[MAXOBS + j].rcv = 2;
				}
			}
			else if (ret == 5)
			{
				xyz_bas[3] = rtcm_obs->sta.pos[0];
				xyz_bas[1] = rtcm_obs->sta.pos[1];
				xyz_bas[2] = rtcm_obs->sta.pos[2];
			}
			else if (ret == 2 && (sat = rtcm_obs->ephsat) > 0)
			{
				if (nav_lock.try_lock())
				{
					sys = satsys(sat, &prn);
					if (sys == SYS_GLO)
					{
						int loc = prn - 1;
						rtcm_nav->nav.geph[loc] = rtcm_obs->nav.geph[loc];
					}
					else if (sys == SYS_GPS || sys == SYS_GAL || sys == SYS_CMP || sys == SYS_QZS || sys == SYS_IRN)
					{
						int loc = sat + MAXSAT * rtcm_obs->ephset - 1;
						rtcm_nav->nav.eph[loc] = rtcm_obs->nav.eph[loc];
					}
				}
			}
		}
		obs_lock.unlock();
		ret = 1;
	}
	return ret;
}
int artk_t::add_brdc(nav_t* nav)
{
	int i = 0;
	int ret = 0;

	if (nav_lock.try_lock())
	{
		for (i = 0; i < MAXSAT + MAXSAT; ++i)
		{
			if (nav->eph[i].sat > 0)
			{
				rtcm_nav->nav.eph[i] = nav->eph[i];
			}
		}
		for (i = 0; i < MAXPRNGLO; ++i)
		{
			if (nav->geph[i].sat > 0)
			{
				rtcm_nav->nav.geph[i] = nav->geph[i];
			}
		}
		nav_lock.unlock();
		ret = 1;
	}
	return ret;
}
int artk_t::add_brdc(char* buff, int nlen)
{
	int i = 0;
	int ret = 0;

	if (nav_lock.try_lock())
	{
		for (i = 0; i < nlen; ++i)
		{
			ret = input_rtcm3(rtcm_nav, (uint8_t)buff[i]);
		}
		nav_lock.unlock();
		ret = 1;
	}
	return ret;
}
int artk_t::proc(char* gga, char *sol)
{
	int ret = 0;
	int i = 0;
	if (rtk_lock.try_lock())
	{
		obsd_t* cur_obs = new obsd_t[MAXOBS + MAXOBS];
		memset(cur_obs, 0, sizeof(obsd_t) * (MAXOBS + MAXOBS));
		double pos_rov[3] = { 0 };
		double pos_bas[3] = { 0 };
		int nobs = 0;
		int nrov = 0;
		int nbas = 0;
		if (obs_lock.try_lock())
		{
			for (i = 0; i < MAXOBS; ++i)
			{
				if (obs[i].rcv == 1)
				{
					cur_obs[nobs++] = obs[i];
					nrov++;
				}
			}
			memcpy(pos_rov, xyz, sizeof(double) * 3);
			for (i = MAXOBS; i < MAXOBS + MAXOBS; ++i)
			{
				if (obs[i].rcv == 2)
				{
					cur_obs[nobs++] = obs[i];
					nbas++;
				}
			}
			memcpy(pos_bas, xyz + 3, sizeof(double) * 3);
			obs_lock.unlock();
		}
		if (fabs(pos_rov[0]) < 0.001 || fabs(pos_rov[1]) < 0.001 || fabs(pos_rov[2]) < 0.001)
		{
			pos_rov[0] = pos_bas[0];
			pos_rov[1] = pos_bas[1];
			pos_rov[2] = pos_bas[2];
		}
		if (nav_lock.try_lock())
		{
			if (fabs(rtk->sol.rr[0]) < 0.001 || fabs(rtk->sol.rr[1]) < 0.001 || fabs(rtk->sol.rr[2]) < 0.001)
			{
				rtk->sol.rr[0] = pos_rov[0];
				rtk->sol.rr[1] = pos_rov[1];
				rtk->sol.rr[2] = pos_rov[2];
			}
			if (fabs(pos_bas[0]) < 0.001 || fabs(pos_bas[1]) < 0.001 || fabs(pos_bas[2]) < 0.001)
			{

			}
			else
			{
				rtk->rb[0] = pos_bas[0];
				rtk->rb[1] = pos_bas[1];
				rtk->rb[2] = pos_bas[2];
			}

			rtk->opt.outsingle = 1;

			rtkpos(rtk, cur_obs, nobs, &rtcm_nav->nav);

			ret = outnmea_gga((uint8_t*)gga, &rtk->sol);

			if (rtk->sol.stat == SOLQ_FIX || rtk->sol.stat == SOLQ_FLOAT)
			{
				double dxyz[3] = { rtk->sol.rr[0] - rtk->rb[0], rtk->sol.rr[1] - rtk->rb[1], rtk->sol.rr[2] - rtk->rb[2] };
				double dist = sqrt(dxyz[0] * dxyz[0] + dxyz[1] * dxyz[1] + dxyz[2] * dxyz[2]) / 1000.0;

				if (dist < 30.0) /* only turn on AMB for baseline < 30 km */
				{
					rtk->opt.modear = 3;
				}
				else
				{
					rtk->opt.modear = 0;
				}

				if (sol)
				{
					int wk = 0;
					double ws = time2gpst(rtk->sol.time, &wk);

					double blh[3] = { 0 };

					ecef2pos(rtk->rb, blh);

					double C_en[3][3] = { 0 };
					double lat = blh[0];
					double lon = blh[1];

					C_en[0][0] = -sin(lat) * cos(lon);
					C_en[1][0] = -sin(lat) * sin(lon);
					C_en[2][0] = cos(lat);
					C_en[0][1] = -sin(lon);
					C_en[1][1] = cos(lon);
					C_en[2][1] = 0.0;
					C_en[0][2] = -cos(lat) * cos(lon);
					C_en[1][2] = -cos(lat) * sin(lon);
					C_en[2][2] = -sin(lat);

					double dned[3] = { 0 };

					/* dNED = C_en'*dXYZ */

					dned[0] = C_en[0][0] * dxyz[0] + C_en[1][0] * dxyz[1] + C_en[2][0] * dxyz[2];
					dned[1] = C_en[0][1] * dxyz[0] + C_en[1][1] * dxyz[1] + C_en[2][1] * dxyz[2];
					dned[2] = C_en[0][2] * dxyz[0] + C_en[1][2] * dxyz[1] + C_en[2][2] * dxyz[2];

					sprintf(sol, "%4i,%10.3f,%10.3f,%i,%14.4f,%14.4f,%14.4f,%14.4f,%14.4f,%14.4f,%3i,%10.3f,%10.3f\r\n", wk, ws, dist, rtk->sol.stat, rtk->rb[0], rtk->rb[1], rtk->rb[2], dned[0], dned[1], dned[2], rtk->sol.ns, rtk->sol.age, rtk->sol.ratio);
				}
			}

			nav_lock.unlock();
		}

		delete[] cur_obs;
		rtk_lock.unlock();
	}
	return ret;
}

