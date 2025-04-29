#include "srtk.h"
#include <map>
#include <vector>
#include <algorithm>
#include <string>

/* help text -----------------------------------------------------------------*/
static const char* help[] = {
"",
" usage: srtkpp rove=rovefilename base=basefilename brdc=brdcfname date=yyyy/mm/dd json=coordfname",
"",
};

/* show message --------------------------------------------------------------*/
extern int showmsg(const char* format, ...)
{
    va_list arg;
    va_start(arg, format); vfprintf(stderr, format, arg); va_end(arg);
    fprintf(stderr, "\r");
    return 0;
}
extern void settspan(gtime_t ts, gtime_t te) {}
extern void settime(gtime_t time) {}

/* print help ----------------------------------------------------------------*/
static void printhelp(void)
{
    int i;
    for (i = 0; i < (int)(sizeof(help) / sizeof(*help)); i++) fprintf(stderr, "%s\n", help[i]);
    exit(0);
}

static void set_output_file_name(const char* fname, const char* key, char* outfname)
{
    char filename[255] = { 0 }, outfilename[255] = { 0 };
    strcpy(filename, fname);
    char* temp = strrchr(filename, '.');
    if (temp) temp[0] = '\0';
    sprintf(outfname, "%s%s", filename, key);
}

static void set_output_file_name2(const char* fname1, const char *fname2, const char* key, char* outfname)
{
    char filename[512] = { 0 }, outfilename[512] = { 0 };
    const char* f1 = strrchr(fname1, '.');
    if (f1)
        strncpy(filename, fname1, strlen(fname1) - strlen(f1));
    else
        strcpy(filename, fname1);
    filename[strlen(filename)] = '-';
    const char* f2 = strrchr(fname2, '\\');
    if (f2)
        strncpy(filename + strlen(filename), f2 + 1, strlen(f2) - 1);
    else if (f2 = strrchr(fname2, '/'))
        strncpy(filename + strlen(filename), f2 + 1, strlen(f2) - 1);
    else
        strcpy(filename + strlen(filename), fname2);
    char* temp = strrchr(filename, '.');
    if (temp) temp[0] = '\0';
    sprintf(outfname, "%s%s", filename, key);
}

static FILE* set_output_file(const char* fname, const char* key)
{
    char filename[255] = { 0 };
    set_output_file_name(fname, key, filename);
    return fopen(filename, "w");
}

static FILE* set_output_file2(const char* fname1, const char* fname2, const char* key)
{
    char filename[255] = { 0 };
    set_output_file_name2(fname1, fname2, key, filename);
    return fopen(filename, "w");
}


int is_skip_year(int year)
{
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
}

int day_of_year(int year, int mon, int day)
{
    int totalDay = day;
    int dayPerMon[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    for (int monIndex = 0; monIndex < mon - 1; ++monIndex)
        totalDay += dayPerMon[monIndex];
    if (mon > 2 && is_skip_year(year)) ++totalDay;
    return totalDay;
}

double get_epoch(int year, int mon, int day)
{
    int doy = day_of_year(year, mon, day);
    int tday = is_skip_year(year) ? 366 : 365;
    return year + ((double)(doy)) / tday;
}

inline bool operator==(const gtime_t& l, const gtime_t& r) {
    return fabs(timediff(l, r)) <= DTTOL;
}
inline bool operator<(const gtime_t& l, const gtime_t& r) {
    return timediff(l, r) < -DTTOL;
}
inline bool operator<=(const gtime_t& l, const gtime_t& r) {
    return l < r || l == r;
}
inline bool operator==(const eph_t& l, const eph_t& r) {
    return l.sat == r.sat && l.toe == r.toe;
}
inline bool operator==(const geph_t& l, const geph_t& r) {
    return l.sat == r.sat && l.toe == r.toe;
}
inline bool operator<(const eph_t& l, const eph_t& r) {
    return l.sat == r.sat ? l.toe < r.toe : l.sat < r.sat;
}
inline bool operator<(const eph_t& l, const gtime_t& r) {
    return l.toe < r;
}
inline bool operator<(const geph_t& l, const geph_t& r) {
    return l.sat == r.sat ? l.toe < r.toe : l.sat < r.sat;
}
inline bool operator<(const geph_t& l, const gtime_t& r) {
    return l.toe < r;
}

static int get_current_date(int& year, int& mon)
{
    time_t now = time(0);
    tm* gmtm = gmtime(&now);
    year = gmtm->tm_year + 1900;
    mon = gmtm->tm_mon + 1;
    return gmtm->tm_mday;
}

struct brdc_t
{
    std::map<int, std::vector< eph_t>> mSatEph;
    std::map<int, std::vector<geph_t>> mGloEph;
    brdc_t()
    {

    }
    int add_sat_eph(int key, const eph_t& eph)
    {
        int ret = 0;
        std::vector<eph_t>::iterator pEph = std::find(mSatEph[key].begin(), mSatEph[key].end(), eph);
        if (pEph == mSatEph[key].end())
        {
            mSatEph[key].push_back(eph);
            std::sort(mSatEph[key].begin(), mSatEph[key].end());
            ret = 1;
        }
        return ret;
    }
    int add_glo_eph(int key, const geph_t& eph)
    {
        int ret = 0;
        std::vector<geph_t>::iterator pEph = std::find(mGloEph[key].begin(), mGloEph[key].end(), eph);
        if (pEph == mGloEph[key].end())
        {
            mGloEph[key].push_back(eph);
            std::sort(mGloEph[key].begin(), mGloEph[key].end());
            ret = 1;
        }
        return ret;
    }
    int read_rtcm_data(const char* fname, int year, int mon, int day)
    {
        int ret = 0;
        int data = 0;
        int sat = 0;
        int sys = 0;
        int prn = 0;
        int sel = 0;
        int count = 0;
        double ep[6] = { 0 };
        if (year <= 0 || mon <= 0 || day <= 0) day = get_current_date(year, mon);
        ep[0] = year;
        ep[1] = mon;
        ep[2] = day;
        FILE* fRTCM = fname ? fopen(fname, "rb") : NULL;
        rtcm_t* rtcm = new rtcm_t;
        init_rtcm(rtcm);
        rtcm->time = rtcm->time_s = epoch2time(ep);
        while (fRTCM && !feof(fRTCM) && (data = fgetc(fRTCM)) != EOF)
        {
            ret = input_rtcm3(rtcm, data);
            if (ret == 2 && (sat = rtcm->ephsat) > 0)
            {
                sys = satsys(sat, &prn);
                if (sys == SYS_GLO)
                {
                    int loc = prn - 1;
                    if (add_glo_eph(loc, rtcm->nav.geph[loc]))
                        ++count;
                }
                else if (sys == SYS_GPS || sys == SYS_GAL || sys == SYS_CMP || sys == SYS_QZS || sys == SYS_IRN)
                {
                    int loc = sat + MAXSAT * rtcm->ephset - 1;
                    if (add_sat_eph(loc, rtcm->nav.eph[loc]))
                        ++count;
                }
            }
        }
        if (fRTCM) fclose(fRTCM);
        free_rtcm(rtcm);
        delete rtcm;
        return ret;
    }
    int get_brdc_data(gtime_t time, nav_t* nav)
    {
        int ret = 1;
        double dt0 = 0;
        double dt1 = 0;
        int is_new = 0;
        /* */
        for (std::map<int, std::vector< eph_t>>::iterator pSatEph = mSatEph.begin(); pSatEph != mSatEph.end(); ++pSatEph)
        {
            if (nav->eph[pSatEph->first].sat > 0)
            {
                dt0 = fabs(timediff(nav->eph[pSatEph->first].toe, time));
            }
            else
            {
                dt0 = 24 * 3600.0;
            }
            std::vector<eph_t>::iterator pEph = std::lower_bound(pSatEph->second.begin(), pSatEph->second.end(), time);
            if (pEph != pSatEph->second.end())
            {
                is_new = 0;
                if ((dt1 = fabs(timediff(pEph->toe, time))) < dt0)
                {
                    dt0 = dt1;
                    nav->eph[pSatEph->first] = *pEph;
                    is_new++;
                }
                if (pEph != pSatEph->second.begin())
                {
                    dt1 = fabs(timediff((pEph - 1)->toe, time));
                    if (dt1 < dt0)
                    {
                        dt0 = dt1;
                        nav->eph[pSatEph->first] = *(pEph - 1);
                        is_new++;
                    }
                }
                if ((pEph + 1) != pSatEph->second.end())
                {
                    dt1 = fabs(timediff((pEph + 1)->toe, time));
                    if (dt1 < dt0)
                    {
                        dt0 = dt1;
                        nav->eph[pSatEph->first] = *(pEph - 1);
                        is_new++;
                    }
                }
                if (is_new) ++ret;
            }
            else if (pSatEph->second.size() > 0)
            {
                --pEph;
                if (pEph != pSatEph->second.end())
                {
                    is_new = 0;
                    if ((dt1 = fabs(timediff(pEph->toe, time))) < dt0)
                    {
                        dt0 = dt1;
                        nav->eph[pSatEph->first] = *pEph;
                        is_new++;
                    }
                    if (is_new) ++ret;
                }
            }
        }
        for (std::map<int, std::vector<geph_t>>::iterator pSatEph = mGloEph.begin(); pSatEph != mGloEph.end(); ++pSatEph)
        {
            if (nav->geph[pSatEph->first].sat > 0)
            {
                dt0 = fabs(timediff(nav->geph[pSatEph->first].toe, time));
            }
            else
            {
                dt0 = 24 * 3600.0;
            }
            std::vector<geph_t>::iterator pEph = std::lower_bound(pSatEph->second.begin(), pSatEph->second.end(), time);
            if (pEph != pSatEph->second.end())
            {
                is_new = 0;
                if ((dt1 = fabs(timediff(pEph->toe, time))) < dt0)
                {
                    dt0 = dt1;
                    nav->geph[pSatEph->first] = *pEph;
                    is_new++;
                }
                if (pEph != pSatEph->second.begin())
                {
                    dt1 = fabs(timediff((pEph - 1)->toe, time));
                    if (dt1 < dt0)
                    {
                        dt0 = dt1;
                        nav->geph[pSatEph->first] = *(pEph - 1);
                        is_new++;
                    }
                }
                if ((pEph + 1) != pSatEph->second.end())
                {
                    dt1 = fabs(timediff((pEph + 1)->toe, time));
                    if (dt1 < dt0)
                    {
                        dt0 = dt1;
                        nav->geph[pSatEph->first] = *(pEph - 1);
                        is_new++;
                    }
                }
                if (is_new) ++ret;
            }
            else if (pSatEph->second.size() > 0)
            {
                --pEph;
                if (pEph != pSatEph->second.end())
                {
                    is_new = 0;
                    if ((dt1 = fabs(timediff(pEph->toe, time))) < dt0)
                    {
                        dt0 = dt1;
                        nav->geph[pSatEph->first] = *pEph;
                        is_new++;
                    }
                    if (is_new) ++ret;
                }
            }
        }
        return 1;
    }
};

struct station_t
{
    FILE* fRTCM;
    rtcm_t* rtcm;
    station_t()
    {
        fRTCM = NULL;
        rtcm = new rtcm_t;
        init_rtcm(rtcm);
    }
    int open(const char* fname, int year, int mon, int day)
    {
        double ep[6] = { 0 };
        if (year <= 0 || mon <= 0 || day <= 0) day = get_current_date(year, mon);
        ep[0] = year;
        ep[1] = mon;
        ep[2] = day;
        rtcm->time = rtcm->time_s = epoch2time(ep);
        if (fRTCM) fclose(fRTCM); fRTCM = NULL;
        fRTCM = fname ? fopen(fname, "rb") : NULL;
        return fRTCM != NULL;
    }
    ~station_t()
    {
        if (fRTCM) fclose(fRTCM);
        free_rtcm(rtcm);
        delete rtcm;
    }
    int get_obs(obsd_t* obs, double* pos)
    {
        int ret = 0;
        int data = 0;
        int nsat = 0;
        while (fRTCM && !feof(fRTCM) && (data = fgetc(fRTCM)) != EOF)
        {
            ret = input_rtcm3(rtcm, data);
            if (ret == 1)
            {
                for (int i = 0; i < rtcm->obs.n; ++i)
                {
                    obs[nsat] = rtcm->obs.data[i];
                    ++nsat;
                }
                pos[0] = rtcm->sta.pos[0];
                pos[1] = rtcm->sta.pos[1];
                pos[2] = rtcm->sta.pos[2];
                break;
            }
        }
        return nsat;
    }
};

struct coord_t
{
    std::string name;
    std::string coord_system_name;
    double epoch;
    double xyz[3];
    double sigma95_xyz[3];
    double amb_fix_rate;
    coord_t()
    {
        epoch = 0;
        xyz[0] = xyz[1] = xyz[2] = 0;
        sigma95_xyz[0] = sigma95_xyz[1] = sigma95_xyz[2] = 0;
        amb_fix_rate = 0;
    }
};

int remove_lead(char* buffer)
{
    size_t nlen = 0;
    while ((nlen = strlen(buffer)) > 0)
    {
        if (buffer[0] == '\'') buffer[0] = ' ';
        if (buffer[0] == '\"') buffer[0] = ' ';
        if (buffer[0] == ' ')
        {
            std::rotate(buffer + 0, buffer + 1, buffer + nlen);
            if (nlen > 0) buffer[nlen - 1] = '\0';
        }
        else
        {
            break;
        }
    }
    while ((nlen = strlen(buffer)) > 0)
    {
        if (buffer[nlen - 1] == '\'' || buffer[nlen - 1] == '\"' || buffer[nlen - 1] == ',' || buffer[nlen - 1] == ' ')
        {
            buffer[nlen - 1] = '\0';
        }
        else
        {
            break;
        }
    }
    return 1;
}

static int read_json_file(const char* fname, std::map<std::string, coord_t>& mCoords)
{
    int ret = 0;
    FILE* fJSON = fopen(fname, "r");
    char buffer[512] = { 0 };
    int is_name_found = 0;
    int is_itrf2020_found = 0;
    int is_itrf2020_2015_found = 0;
    int is_wgs84_found = 0;
    int is_regional_found = 0;
    int is_x = 0;
    int is_y = 0;
    int is_z = 0;
    char* temp = NULL;
    char* temp1 = NULL;

    std::string name;
    double sigma95_xyz[3] = { 0 };
    double xyz_itrf2020[3] = { 0 };
    double vxyz_itrf2020[3] = { 0 };
    double xyz_regional[3] = { 0 };
    double vxyz_regional[3] = { 0 };
    double xyz_itrf2020_2015[3] = { 0 };
    double xyz_wgs84[3] = { 0 };
    double vxyz_wgs84[3] = { 0 };
    double amb_fix_rate = 0;
    double epoch_itrf2020 = 0;
    double epoch_itrf2020_2015 = 0;
    double epoch_regional = 0;
    double epoch_wgs84 = 0;
    std::string stime;
    std::string ctime;
    std::string coord_name_itrf2020;
    std::string coord_name_itrf2020_2015;
    std::string coord_name_regional;
    std::string coord_name_wgs84;

    while (fJSON && !feof(fJSON) && fgets(buffer, sizeof(buffer), fJSON))
    {
        //printf("%s", buffer);
        if (temp = strchr(buffer, '\n')) temp[0] = '\0';
        remove_lead(buffer);
        if (is_name_found)
        {
            if (is_itrf2020_found == 1)
            {
                if ((temp = strstr(buffer, "x:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vx:"))
                        vxyz_itrf2020[0] = atof(temp1 + 1);
                    else
                        xyz_itrf2020[0] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "y:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vy:"))
                        vxyz_itrf2020[1] = atof(temp1 + 1);
                    else
                        xyz_itrf2020[1] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "z:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vz:"))
                        vxyz_itrf2020[2] = atof(temp1 + 1);
                    else
                        xyz_itrf2020[2] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "iar:")) && (temp1 = strstr(temp, ":")))
                {
                    amb_fix_rate = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "epoch:")) && (temp1 = strstr(temp, ":")))
                {
                    epoch_itrf2020 = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "sigma95X:")) && (temp1 = strstr(temp, ":")))
                {
                    sigma95_xyz[0] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "sigma95Y:")) && (temp1 = strstr(temp, ":")))
                {
                    sigma95_xyz[1] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "sigma95Z:")) && (temp1 = strstr(temp, ":")))
                {
                    sigma95_xyz[2] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "dateObserved:")) && (temp1 = strstr(temp, ":")))
                {
                    remove_lead(temp1 + 1);
                    char* temp11 = strchr(temp1 + 1, ' ');
                    if (temp11) temp11[0] = '\0';
                    stime = std::string(temp1 + 1);
                }
                if ((temp = strstr(buffer, "dateComputed:")) && (temp1 = strstr(temp, ":")))
                {
                    remove_lead(temp1 + 1);
                    char* temp11 = strchr(temp1 + 1, ' ');
                    if (temp11) temp11[0] = '\0';
                    ctime = std::string(temp1 + 1);
                }
                if ((temp = strstr(buffer, "name:")) && (temp1 = strstr(temp, ":")))
                {
                    remove_lead(temp1 + 1);
                    coord_name_itrf2020 = std::string(temp1 + 1);
                }
                if (strstr(buffer, "}"))
                {
                    is_itrf2020_found = 2;
                }
            }
            else if (!is_itrf2020_found && strstr(buffer, "itrf2020:"))
            {
                is_itrf2020_found = 1;
            }
            if (is_itrf2020_2015_found == 1)
            {
                if (!strstr(buffer, "vx:") && (temp = strstr(buffer, "x:")) && (temp1 = strstr(temp, ":")))
                {
                    xyz_itrf2020_2015[0] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "vx:")) && (temp1 = strstr(temp, ":")))
                {
                    vxyz_itrf2020[0] = atof(temp1 + 1);
                }
                if (!strstr(buffer, "vy:") && (temp = strstr(buffer, "y:")) && (temp1 = strstr(temp, ":")))
                {
                    xyz_itrf2020_2015[1] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "vy:")) && (temp1 = strstr(temp, ":")))
                {
                    vxyz_itrf2020[1] = atof(temp1 + 1);
                }
                if (!strstr(buffer, "vz:") && (temp = strstr(buffer, "z:")) && (temp1 = strstr(temp, ":")))
                {
                    xyz_itrf2020_2015[2] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "vz:")) && (temp1 = strstr(temp, ":")))
                {
                    vxyz_itrf2020[2] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "name:")) && (temp1 = strstr(temp, ":")))
                {
                    remove_lead(temp1 + 1);
                    coord_name_itrf2020_2015 = std::string(temp1 + 1);
                    std::size_t nloc = coord_name_itrf2020_2015.find_last_of('(');
                    if (nloc != std::string::npos)
                    {
                        epoch_itrf2020_2015 = atof(coord_name_itrf2020_2015.substr(nloc + 1).c_str());
                    }
                    else
                    {
                        epoch_itrf2020_2015 = atof(coord_name_itrf2020_2015.c_str());
                    }
                }
                if (strstr(buffer, "}"))
                {
                    is_itrf2020_2015_found = 2;
                }
            }
            else if (!is_itrf2020_2015_found && strstr(buffer, "itrf2015:"))
            {
                is_itrf2020_2015_found = 1;
            }
            if (is_wgs84_found == 1)
            {
                if ((temp = strstr(buffer, "x:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vx:"))
                        vxyz_wgs84[0] = atof(temp1 + 1);
                    else
                        xyz_wgs84[0] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "y:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vy:"))
                        vxyz_wgs84[1] = atof(temp1 + 1);
                    else
                        xyz_wgs84[1] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "z:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vz:"))
                        vxyz_wgs84[2] = atof(temp1 + 1);
                    else
                        xyz_wgs84[2] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "name:")) && (temp1 = strstr(temp, ":")))
                {
                    remove_lead(temp1 + 1);
                    coord_name_wgs84 = std::string(temp1 + 1);
                    std::size_t nloc = coord_name_wgs84.find_last_of('(');
                    if (nloc != std::string::npos)
                    {
                        epoch_wgs84 = atof(coord_name_wgs84.substr(nloc + 1).c_str());
                    }
                    else
                    {
                        epoch_wgs84 = atof(coord_name_wgs84.c_str());
                    }
                }
                if (strstr(buffer, "}"))
                {
                    is_wgs84_found = 2;
                }
            }
            else if (!is_wgs84_found && strstr(buffer, "wgs84:"))
            {
                is_wgs84_found = 1;
            }

            if (is_regional_found == 1)
            {
                if ((temp = strstr(buffer, "x:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vx:"))
                        vxyz_regional[0] = atof(temp1 + 1);
                    else
                        xyz_regional[0] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "y:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vy:"))
                        vxyz_regional[1] = atof(temp1 + 1);
                    else
                        xyz_regional[1] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "z:")) && (temp1 = strstr(temp, ":")))
                {
                    if (strstr(buffer, "vz:"))
                        vxyz_regional[2] = atof(temp1 + 1);
                    else
                        xyz_regional[2] = atof(temp1 + 1);
                }
                if ((temp = strstr(buffer, "name:")) && (temp1 = strstr(temp, ":")))
                {
                    remove_lead(temp1 + 1);
                    coord_name_regional = std::string(temp1 + 1);
                    std::size_t nloc = coord_name_regional.find_last_of('(');
                    if (nloc != std::string::npos)
                    {
                        epoch_regional = atof(coord_name_regional.substr(nloc + 1).c_str());
                    }
                    else
                    {
                        epoch_regional = atof(coord_name_regional.c_str());
                    }
                }
                if (strstr(buffer, "}"))
                {
                    is_regional_found = 2;
                }
            }
            else if (!is_regional_found && strstr(buffer, "regional:"))
            {
                is_regional_found = 1;
            }
            if (name.size() > 0 && is_itrf2020_found == 2 && is_itrf2020_2015_found == 2 && is_wgs84_found == 2 && is_regional_found == 2)
            {
                double vel3D = sqrt(vxyz_itrf2020[0] * vxyz_itrf2020[0] + vxyz_itrf2020[1] * vxyz_itrf2020[1] + vxyz_itrf2020[2] * vxyz_itrf2020[2]);
                if (vel3D > 0.5)
                {
                    vxyz_itrf2020[0] /= 1000.0;
                    vxyz_itrf2020[1] /= 1000.0;
                    vxyz_itrf2020[2] /= 1000.0;
                }
                double vel3D_r = sqrt(vxyz_regional[0] * vxyz_regional[0] + vxyz_regional[1] * vxyz_regional[1] + vxyz_regional[2] * vxyz_regional[2]);
                if (vel3D_r > 0.5)
                {
                    vxyz_regional[0] /= 1000.0;
                    vxyz_regional[1] /= 1000.0;
                    vxyz_regional[2] /= 1000.0;
                }
                double vel3D_wgs84 = sqrt(vxyz_wgs84[0] * vxyz_wgs84[0] + vxyz_wgs84[1] * vxyz_wgs84[1] + vxyz_wgs84[2] * vxyz_wgs84[2]);
                if (vel3D_wgs84 > 0.5)
                {
                    vxyz_wgs84[0] /= 1000.0;
                    vxyz_wgs84[1] /= 1000.0;
                    vxyz_wgs84[2] /= 1000.0;
                }
                /* add */
                coord_t coord;
                coord.name = name;
                coord.coord_system_name = coord_name_itrf2020;
                coord.epoch = epoch_itrf2020;
                coord.amb_fix_rate = amb_fix_rate;
                coord.xyz[0] = xyz_itrf2020[0];
                coord.xyz[1] = xyz_itrf2020[1];
                coord.xyz[2] = xyz_itrf2020[2];
                coord.sigma95_xyz[0] = sigma95_xyz[0];
                coord.sigma95_xyz[1] = sigma95_xyz[1];
                coord.sigma95_xyz[2] = sigma95_xyz[2];
                mCoords[name] = coord;
                /* reset */
                name.clear();
                memset(sigma95_xyz, 0, sizeof(sigma95_xyz));
                memset(xyz_itrf2020, 0, sizeof(xyz_itrf2020));
                memset(vxyz_itrf2020, 0, sizeof(vxyz_itrf2020));
                memset(xyz_regional, 0, sizeof(xyz_regional));
                memset(vxyz_regional, 0, sizeof(vxyz_regional));
                memset(xyz_itrf2020_2015, 0, sizeof(xyz_itrf2020_2015));
                memset(xyz_wgs84, 0, sizeof(xyz_wgs84));
                memset(vxyz_wgs84, 0, sizeof(vxyz_wgs84));

                amb_fix_rate = 0;
                epoch_itrf2020 = 0;
                epoch_itrf2020_2015 = 0;
                epoch_regional = 0;
                epoch_wgs84 = 0;
                stime.clear();
                ctime.clear();
                coord_name_itrf2020.clear();
                coord_name_itrf2020_2015.clear();
                coord_name_regional.clear();
                coord_name_wgs84.clear();
                /* find next one */
                is_name_found = 0;
            }
        }
        else if ((temp = strstr(buffer, "name:")) && (temp1 = strstr(temp, ":")))
        {
            remove_lead(temp1 + 1);
            name = std::string(temp1 + 1);
            is_name_found = 1;
            is_itrf2020_found = 0;
            is_itrf2020_2015_found = 0;
            is_wgs84_found = 0;
            is_regional_found = 0;
        }
    }
    if (fJSON) fclose(fJSON);
    return ret;
}

struct solu_t
{
    int wk;
    double ws;
    int nsat;
    int type;
    double xyz[3];
    double ned[3];
    double age;
    double ratio;
    solu_t()
    {
        wk = 0;
        ws = 0;
        nsat = 0;
        type = 0;
        xyz[0] = xyz[1] = xyz[2] = 0;
        ned[0] = ned[1] = ned[2] = 0;
        age = 0;
        ratio = 0;
    }
    int parse(char* sol)
    {
        int ret = 0;
        double dist = 0;
        int n = sscanf(sol, "%i,%lf,%lf,%i,%lf,%lf,%lf,%lf,%lf,%lf,%i,%lf,%lf", &wk, &ws, &dist, &type, xyz + 0, xyz + 1, xyz + 2, ned + 0, ned + 1, ned + 2, &nsat, &age, &ratio);
        if (n >= 13)
        {
            ret = 1;
        }
        return ret;
    }
};

static int solution_status(std::vector<solu_t>& solu, double *msol, int *nfix, double *sfix, int *nrtk, double *srtk)
{
    int ret = 0;
    double mfix[3] = { 0 };
    double mrtk[3] = { 0 };
    *nfix = 0;
    *nrtk = 0;
    for (std::vector<solu_t>::iterator pSolu = solu.begin(); pSolu != solu.end(); ++pSolu)
    {
        if (pSolu->type == 1 || pSolu->type == 2)
        {
            if (pSolu->type == 1)
            {
                mfix[0] += pSolu->ned[0];
                mfix[1] += pSolu->ned[1];
                mfix[2] += pSolu->ned[2];
                ++(*nfix);
            }
            mrtk[0] += pSolu->ned[0];
            mrtk[1] += pSolu->ned[1];
            mrtk[2] += pSolu->ned[2];
            ++(*nrtk);
        }
    }
    if (*nrtk > 60)
    {
        mrtk[0] /= *nrtk;
        mrtk[1] /= *nrtk;
        mrtk[2] /= *nrtk;
        ret = 1;
        if (*nfix > 60)
        {
            mfix[0] /= *nfix;
            mfix[1] /= *nfix;
            mfix[2] /= *nfix;
            msol[0] = mfix[0];
            msol[1] = mfix[1];
            msol[2] = mfix[2];
            ret = 2;
        }
        else
        {
            msol[0] = mrtk[0];
            msol[1] = mrtk[1];
            msol[2] = mrtk[2];
        }
        sfix[0] = sfix[1] = sfix[2] = 0;
        srtk[0] = srtk[1] = srtk[2] = 0;
        for (std::vector<solu_t>::iterator pSolu = solu.begin(); pSolu != solu.end(); ++pSolu)
        {
            if (pSolu->type == 1 || pSolu->type == 2)
            {
                if (pSolu->type == 1)
                {
                    sfix[0] += (pSolu->ned[0] - msol[0]) * (pSolu->ned[0] - msol[0]);
                    sfix[1] += (pSolu->ned[1] - msol[1]) * (pSolu->ned[1] - msol[1]);
                    sfix[2] += (pSolu->ned[2] - msol[2]) * (pSolu->ned[2] - msol[2]);
                }
                srtk[0] += (pSolu->ned[0] - msol[0]) * (pSolu->ned[0] - msol[0]);
                srtk[1] += (pSolu->ned[1] - msol[1]) * (pSolu->ned[1] - msol[1]);
                srtk[2] += (pSolu->ned[2] - msol[2]) * (pSolu->ned[2] - msol[2]);
            }
        }
        srtk[0] = sqrt(srtk[0] / (*nrtk - 1));
        srtk[1] = sqrt(srtk[1] / (*nrtk - 1));
        srtk[2] = sqrt(srtk[2] / (*nrtk - 1));
        if (*nfix > 60)
        {
            sfix[0] = sqrt(sfix[0] / (*nfix - 1));
            sfix[1] = sqrt(sfix[1] / (*nfix - 1));
            sfix[2] = sqrt(sfix[2] / (*nfix - 1));
        }
    }
    return ret;
}


static int process_log(const char* rovefname, const char* basefname, const char* brdcfname, int year, int mm, int dd, coord_t &base_coord)
{
    int ret = 0;
    artk_t* artk = new artk_t;

    brdc_t* brdc = new brdc_t;
    brdc->read_rtcm_data(rovefname, year, mm, dd);
    brdc->read_rtcm_data(basefname, year, mm, dd);
    brdc->read_rtcm_data(brdcfname, year, mm, dd);

    station_t* base = new station_t;
    station_t* rove = new station_t;

    rove->open(rovefname, year, mm, dd);
    base->open(basefname, year, mm, dd);

    obsd_t* obsd = new obsd_t[MAXOBS + MAXOBS];
    memset(obsd, 0, sizeof(obsd_t) * (MAXOBS + MAXOBS));
    int nsat[2] = { 0 };
    double pos[12] = { 0 };
    obsd_t* obs_rove = obsd;
    obsd_t* obs_base = obsd + MAXOBS;
    double* pos_rove = pos + 0;
    double* pos_base = pos + 3;
    double* pos_rove0 = pos + 6;
    double* pos_base0 = pos + 9;
    double dxyz[4] = { 0 };

    FILE* fGGA = set_output_file2(rovefname, basefname, ".nmea");
    FILE* fSOL = set_output_file2(rovefname, basefname, ".csv");

    std::string rove_name;
    std::string base_name;

    std::vector<solu_t> vSolu;

    double dt = 0;

    int count = 0;

    if (fSOL)
    {
        fprintf(fSOL, "#time[yyy/mm/dd hh:mm:ss],baseline[km],solu_type[1=>FIX,2=>FLT],x[m],y[m],z[m],diffN[m],diffE[m],diffD[m],nsat,age[s],ratio\r\n");
    }

    while (true)
    {
        nsat[0] = rove->get_obs(obs_rove, pos_rove);
        if (!nsat[0]) break;
        if (fabs(pos_rove[0]) < 0.001 || fabs(pos_rove[1]) < 0.001 || fabs(pos_rove[2]) < 0.001)
        {

        }
        else if (fabs(pos_rove0[0]) < 0.001 || fabs(pos_rove0[1]) < 0.001 || fabs(pos_rove0[2]) < 0.001)
        {
            /* init */
            pos_rove0[0] = pos_rove[0];
            pos_rove0[1] = pos_rove[1];
            pos_rove0[2] = pos_rove[2];
            double blh[3] = { 0 };
            ecef2pos(pos_rove, blh);
            if (fSOL)
            {
                fprintf(fSOL, "#rove rtcm coordinate initial %s %14.9f %14.9f %14.4f %14.4f %14.4f %14.4f\r\n", time_str(obs_rove[0].time, 3), blh[0] * R2D, blh[1] * R2D, blh[2], pos_rove0[0], pos_rove0[1], pos_rove0[2]);
            }
        }
        else
        {
            /* check changes */
            dxyz[0] = pos_rove[0] - pos_rove0[0];
            dxyz[1] = pos_rove[1] - pos_rove0[1];
            dxyz[2] = pos_rove[2] - pos_rove0[2];
            dxyz[3] = sqrt(dxyz[0] * dxyz[0] + dxyz[1] * dxyz[1] + dxyz[2] * dxyz[2]);
            if (dxyz[3] > 5.0)
            {
                double blh[3] = { 0 };
                ecef2pos(pos_rove, blh);
                if (fSOL)
                {
                    fprintf(fSOL, "#rove rtcm coordinate changed %s %14.9f %14.9f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f\r\n", time_str(obs_rove[0].time, 3), blh[0]*R2D, blh[1]*R2D, blh[2], pos_rove0[0], pos_rove0[1], pos_rove0[2], dxyz[0], dxyz[1], dxyz[2], dxyz[3]);
                }
                pos_rove0[0] = pos_rove[0];
                pos_rove0[1] = pos_rove[1];
                pos_rove0[2] = pos_rove[2];
            }
        }
        while (true)
        {
            if (nsat[1] > 0)
            {
                if (obs_rove->time == obs_base->time || obs_rove->time < obs_base->time) break;
                if ((dt = timediff(obs_rove->time, obs_base->time)) > 0.5)
                {
                    nsat[1] = base->get_obs(obs_base, pos_base);
                }
            }
            else
            {
                nsat[1] = base->get_obs(obs_base, pos_base);
            }
            if (!nsat[1]) break;
            if (fabs(pos_base[0]) < 0.001 || fabs(pos_base[1]) < 0.001 || fabs(pos_base[2]) < 0.001)
            {
            }
            else if (fabs(pos_base0[0]) < 0.001 || fabs(pos_base0[1]) < 0.001 || fabs(pos_base0[2]) < 0.001)
            {
                /* init */
                pos_base0[0] = pos_base[0];
                pos_base0[1] = pos_base[1];
                pos_base0[2] = pos_base[2];
                double blh[3] = { 0 };
                ecef2pos(pos_base, blh);
                if (fSOL)
                {
                    fprintf(fSOL, "#base rtcm coordinate initial %s %14.9f %14.9f %14.4f %14.4f %14.4f %14.4f\r\n", time_str(obs_rove[0].time, 3), blh[0] * R2D, blh[1] * R2D, blh[2], pos_base0[0], pos_base0[1], pos_base0[2]);
                }
                pos_base0[0] = pos_base[0];
                pos_base0[1] = pos_base[1];
                pos_base0[2] = pos_base[2];
            }
            else
            {
                /* check changes */
                dxyz[0] = pos_base[0] - pos_base0[0];
                dxyz[1] = pos_base[1] - pos_base0[1];
                dxyz[2] = pos_base[2] - pos_base0[2];
                dxyz[3] = sqrt(dxyz[0] * dxyz[0] + dxyz[1] * dxyz[1] + dxyz[2] * dxyz[2]);
                if (dxyz[3] > 5.0)
                {
                    double blh[3] = { 0 };
                    ecef2pos(pos_base, blh);
                    if (fSOL)
                    {
                        fprintf(fSOL, "#base rtcm coordinate changed %s %14.9f %14.9f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f\r\n", time_str(obs_rove[0].time, 3), blh[0] * R2D, blh[1] * R2D, blh[2], pos_base0[0], pos_base0[1], pos_base0[2], dxyz[0], dxyz[1], dxyz[2], dxyz[3]);
                    }
                    pos_base0[0] = pos_base[0];
                    pos_base0[1] = pos_base[1];
                    pos_base0[2] = pos_base[2];
                }
            }
        }
#if 0
        /* use external coordinate */
        if (rove_name.length() > 0)
        {
            if (fabs(mCoords[rove_name].xyz[0]) < 0.001 || fabs(mCoords[rove_name].xyz[1]) < 0.001 || fabs(mCoords[rove_name].xyz[2]) < 0.001)
            {
            }
            else
            {
                pos_rove[0] = mCoords[rove_name].xyz[0];
                pos_rove[1] = mCoords[rove_name].xyz[1];
                pos_rove[2] = mCoords[rove_name].xyz[2];
            }
        }
        if (base_name.length() > 0)
        {
            if (fabs(mCoords[base_name].xyz[0]) < 0.001 || fabs(mCoords[base_name].xyz[1]) < 0.001 || fabs(mCoords[base_name].xyz[2]) < 0.001)
            {
            }
            else
            {
                pos_base[0] = mCoords[base_name].xyz[0];
                pos_base[1] = mCoords[base_name].xyz[1];
                pos_base[2] = mCoords[base_name].xyz[2];
            }
        }
#endif
        /* base and rove data ready */
        if (nsat[0] > 0 && artk->add_rove_obs(obs_rove, nsat[0], pos_rove0))
        {
            brdc->get_brdc_data(obs_rove[0].time, &rove->rtcm->nav);
            artk->add_brdc(&rove->rtcm->nav);
            if (nsat[1] > 0 && artk->add_base_obs(obs_base, nsat[1], pos_base0))
            {

            }
            else
            {
            }
            char gga[255] = { 0 };
            char sol[255] = { 0 };
            if (artk->proc(gga, sol))
            {
                if (fGGA) { fprintf(fGGA, "%s", gga); fflush(fGGA); }
                if (fSOL) { fprintf(fSOL, "%s", sol); fflush(fSOL); }
                solu_t solu;
                if (solu.parse(sol))
                {
                    vSolu.push_back(solu);
                }
            }
            count++;
        }
    }

    if (vSolu.size()>0)
    {
        double msol[3] = { 0 };
        int nfix = 0;
        int nrtk = 0;
        double sfix[3] = { 0 };
        double srtk[3] = { 0 };

        int status = solution_status(vSolu, msol, &nfix, sfix, &nrtk, srtk);

        double blh[3] = { 0 };
        ecef2pos(vSolu.rbegin()->xyz, blh);

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

        double dxyz[3] = { 0 };

        /* dXYZ = C_en*dNED */

        dxyz[0] = C_en[0][0] * msol[0] + C_en[0][1] * msol[1] + C_en[0][2] * msol[2];
        dxyz[1] = C_en[1][0] * msol[0] + C_en[1][1] * msol[1] + C_en[1][2] * msol[2];
        dxyz[2] = C_en[2][0] * msol[0] + C_en[2][1] * msol[1] + C_en[2][2] * msol[2];

        double covNED[3] = { sfix[0] * sfix[0], sfix[1] * sfix[1], sfix[2] * sfix[2] };
        /* cov(xyz) = C_en*cov(ned)*C_en' */
        double covXYZ[3] = { 0 };
        covXYZ[0] = C_en[0][0] * covNED[0] * C_en[0][0] + C_en[0][1] * covNED[1] * C_en[0][1] + C_en[0][2] * covNED[2] * C_en[0][2];
        covXYZ[1] = C_en[1][0] * covNED[0] * C_en[1][0] + C_en[1][1] * covNED[1] * C_en[1][1] + C_en[1][2] * covNED[2] * C_en[1][2];
        covXYZ[2] = C_en[2][0] * covNED[0] * C_en[2][0] + C_en[2][1] * covNED[1] * C_en[2][1] + C_en[2][2] * covNED[2] * C_en[2][2];

        /* RTK fix accuracy in XYZ */
        double sfix_xyz[3] = { sqrt(covXYZ[0]), sqrt(covXYZ[1]), sqrt(covXYZ[2]) };
        /* get RTK accuracy in XYZ */
        covNED[0] = srtk[0] * srtk[0];
        covNED[1] = srtk[1] * srtk[1];
        covNED[2] = srtk[2] * srtk[2];

        covXYZ[0] = C_en[0][0] * covNED[0] * C_en[0][0] + C_en[0][1] * covNED[1] * C_en[0][1] + C_en[0][2] * covNED[2] * C_en[0][2];
        covXYZ[1] = C_en[1][0] * covNED[0] * C_en[1][0] + C_en[1][1] * covNED[1] * C_en[1][1] + C_en[1][2] * covNED[2] * C_en[1][2];
        covXYZ[2] = C_en[2][0] * covNED[0] * C_en[2][0] + C_en[2][1] * covNED[1] * C_en[2][1] + C_en[2][2] * covNED[2] * C_en[2][2];

        /* RTK accuracy in XYZ */
        double srtk_xyz[3] = { sqrt(covXYZ[0]), sqrt(covXYZ[1]), sqrt(covXYZ[2]) };

        if (base_coord.epoch > 0 && (status==1||status==2))
        {
            gtime_t last_epoch = gpst2time(vSolu.rbegin()->wk, vSolu.rbegin()->ws);
            double ep[6] = { 0 };
            time2epoch(last_epoch, ep);
            
            double cur_epoch = get_epoch((int)ep[0], (int)ep[1], (int)ep[2]);

            double rov_xyz[3] = { base_coord.xyz[0] + dxyz[0], base_coord.xyz[1] + dxyz[1],base_coord.xyz[2] + dxyz[2] };
            double amb_fix_rate = nfix * 100.0 / nrtk;
            char coord_name[255] = { 0 };
            sprintf(coord_name, "ITRF2020(%.2f)", cur_epoch);

            char name[255] = { 0 };
            const char* f1 = strrchr(rovefname, '\\');
            if (f1||(f1 = strrchr(rovefname, '/')))
                strcpy(name, f1 + 1);
            else 
                strcpy(name, rovefname);
            char* f2 = strrchr(name, '.');
            if (f2) f2[0] = '\0';
            std::string rcv_name = std::string(name);
            f2 = strrchr(name, '-');
            if (f2) rcv_name = std::string(f2 + 1);

            if (status == 2 && amb_fix_rate > 50.0)
            {
                if (fSOL) fprintf(fSOL, "#solu,%s,%20s,%15.6f,%7.2f,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f\n", rcv_name.c_str(), coord_name, cur_epoch, amb_fix_rate, rov_xyz[0], rov_xyz[1], rov_xyz[2], sfix_xyz[0], sfix_xyz[1], sfix_xyz[2]);
                printf("solu=%15s,%20s,%15.6f,%7.2f,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f\n", rcv_name.c_str(), coord_name, cur_epoch, amb_fix_rate, rov_xyz[0], rov_xyz[1], rov_xyz[2], sfix_xyz[0], sfix_xyz[1], sfix_xyz[2]);
            }
            else
            {
                if (fSOL) fprintf(fSOL, "#solu,%s,%20s,%15.6f,%7.2f,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f\n", rcv_name.c_str(), coord_name, cur_epoch, amb_fix_rate, rov_xyz[0], rov_xyz[1], rov_xyz[2], srtk_xyz[0], srtk_xyz[1], srtk_xyz[2]);
                printf("solu=%15s,%20s,%15.6f,%7.2f,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f\n", rcv_name.c_str(), coord_name, cur_epoch, amb_fix_rate, rov_xyz[0], rov_xyz[1], rov_xyz[2], srtk_xyz[0], srtk_xyz[1], srtk_xyz[2]);
            }
        }
        if (fSOL)
        {
            fprintf(fSOL, "#stat[1=>FLT NED,2=>FIX NED],dN[m],dE[m],dD[m],std_fix_N[m],std_fix_E[m],std_fix_D[m],count_fix,std_rtk_N[m],std_rtk_E[m],std_rtk_D[m],count_rtk\r\n");
            fprintf(fSOL, "#stat[3=>FLT XYZ,4=>FIX XYZ],dX[m],dY[m],dZ[m],std_fix_X[m],std_fix_Y[m],std_fix_Z[m],count_fix,std_rtk_X[m],std_rtk_Y[m],std_rtk_Z[m],count_rtk\r\n");
            fprintf(fSOL, "#stat=%i,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f,%6i,%10.4f,%10.4f,%10.4f,%6i\n", status, msol[0], msol[1], msol[2], sfix[0], sfix[1], sfix[2], nfix, srtk[0], srtk[1], srtk[2], nrtk);
            fprintf(fSOL, "#stat=%i,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f,%6i,%10.4f,%10.4f,%10.4f,%6i\n", status + 2, dxyz[0], dxyz[1], dxyz[2], sfix_xyz[0], sfix_xyz[1], sfix_xyz[2], nfix, srtk_xyz[0], srtk_xyz[1], srtk_xyz[2], nrtk);
        }
        printf("#ble=%i,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f,%6i,%10.4f,%10.4f,%10.4f,%6i\n", status, msol[0], msol[1], msol[2], sfix[0], sfix[1], sfix[2], nfix, srtk[0], srtk[1], srtk[2], nrtk);
        printf("#ble=%i,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f,%6i,%10.4f,%10.4f,%10.4f,%6i\n", status + 2, dxyz[0], dxyz[1], dxyz[2], sfix_xyz[0], sfix_xyz[1], sfix_xyz[2], nfix, srtk_xyz[0], srtk_xyz[1], srtk_xyz[2], nrtk);
    }

    delete brdc;
    delete base;
    delete rove;
    delete[] obsd;

    delete artk;

    printf("rove=%s\n", rovefname);
    printf("base=%s\n", basefname);
    printf("brdc=%s\n", brdcfname);

    if (fSOL)
    {
        fprintf(fSOL, "#rove=%s\n", rovefname);
        fprintf(fSOL, "#base=%s\n", basefname);
        fprintf(fSOL, "#brdc=%s\n", brdcfname);
    }

    if (fSOL) fclose(fSOL);
    if (fGGA) fclose(fGGA);



    return ret;
}

/* srtkpp main -------------------------------------------------------------*/
int main(int argc, char** argv)
{
    int ret = 0;
    if (argc < 2)
    {
        printhelp();
    }
    else
    {

        int yyyy = 0;
        int mm = 0;
        int dd = 0;
        std::string rovefname;
        std::string basefname;
        std::string brdcfname;
        std::map<std::string, coord_t> mCoords;
        std::string jsonfname;
        char* temp = nullptr;
        for (int i = 1; i < argc; ++i)
        {
            if (temp = strchr(argv[i], '='))
            {
                temp[0] = '\0';
                if (strstr(argv[i], "rove"))
                {
                    rovefname = std::string(temp + 1);
                }
                else if (strstr(argv[i], "base"))
                {
                    basefname = std::string(temp + 1);
                }
                else if (strstr(argv[i], "json"))
                {
                    jsonfname = std::string(temp + 1);
                    read_json_file(jsonfname.c_str(), mCoords);
                }
                else if (strstr(argv[i], "brdc"))
                {
                    brdcfname = std::string(temp + 1);
                }
                else if (strstr(argv[i], "date"))
                {
                    char* temp1 = nullptr;
                    while (temp1 = strchr(temp + 1, '/')) temp1[0] = ' ';
                    while (temp1 = strchr(temp + 1, ':')) temp1[0] = ' ';
                    while (temp1 = strchr(temp + 1, '-')) temp1[0] = ' ';
                    int num = sscanf(temp + 1, "%i %i %i", &yyyy, &mm, &dd);
                    if (num < 3)
                    {
                        yyyy = mm = dd = 0;
                    }
                    else
                    {
                    }
                }
            }

        }
        clock_t stime = clock();

        coord_t rove_coord;
        coord_t base_coord;

        for (std::map<std::string, coord_t>::iterator pCoord = mCoords.begin(); pCoord != mCoords.end(); ++pCoord)
        {
            if (strstr(basefname.c_str(), pCoord->first.c_str()))
            {
                base_coord = pCoord->second;
                printf("base=%15s,%20s,%15.6f,%7.2f,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f\n", pCoord->first.c_str(), pCoord->second.coord_system_name.c_str(), pCoord->second.epoch, pCoord->second.amb_fix_rate, pCoord->second.xyz[0], pCoord->second.xyz[1], pCoord->second.xyz[2], pCoord->second.sigma95_xyz[0], pCoord->second.sigma95_xyz[1], pCoord->second.sigma95_xyz[2]);
                break;
            }
        }
        for (std::map<std::string, coord_t>::iterator pCoord = mCoords.begin(); pCoord != mCoords.end(); ++pCoord)
        {
            if (strstr(rovefname.c_str(), pCoord->first.c_str()))
            {
                rove_coord = pCoord->second;
                printf("rove=%15s,%20s,%15.6f,%7.2f,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f\n", pCoord->first.c_str(), pCoord->second.coord_system_name.c_str(), pCoord->second.epoch, pCoord->second.amb_fix_rate, pCoord->second.xyz[0], pCoord->second.xyz[1], pCoord->second.xyz[2], pCoord->second.sigma95_xyz[0], pCoord->second.sigma95_xyz[1], pCoord->second.sigma95_xyz[2]);
                break;
            }
        }

        ret = process_log(rovefname.c_str(), basefname.c_str(), brdcfname.c_str(), yyyy, mm, dd, base_coord);

        clock_t etime = clock();
        double cpu_time_used = ((double)(etime - stime)) / CLOCKS_PER_SEC;
        printf("time=%.3f[s]\n", cpu_time_used);
    }
    return ret;
}
