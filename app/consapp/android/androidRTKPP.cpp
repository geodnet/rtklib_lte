#include "androidRTK.h"
#include <map>
#include <vector>
#include <algorithm>
#include <string>
#include <filesystem>

/* help text -----------------------------------------------------------------*/
static const char* help[] = {
"",
" usage: androidRTKPP rove=rovefilename base=basefilename brdc=brdcfname date=yyyy/mm/dd json=coordfname",
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

int parse_fields(char* const buffer, char** val, char key, int maxfield)
{
    char* p, * q;
    int n = 0;

    /* parse fields */
    for (p = buffer; *p && n < maxfield; p = q + 1) {
        if (p == NULL) break;
        if ((q = strchr(p, key)) || (q = strchr(p, '\n')) || (q = strchr(p, '\r'))) {
            val[n++] = p; *q = '\0';
        }
        else break;
    }
    if (p) val[n++] = p;
    return n;
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
        for (std::map<int, std::vector<eph_t>>::iterator pSatEph = mSatEph.begin(); pSatEph != mSatEph.end(); ++pSatEph)
        {
            eph_t bestEph = nav->eph[pSatEph->first];
            for (std::vector<eph_t>::iterator pEph = pSatEph->second.begin(); pEph != pSatEph->second.end(); ++pEph)
            {
                dt1 = fabs(timediff(pEph->toe, time));
                if (bestEph.sat > 0)
                {
                    dt0 = fabs(timediff(bestEph.toe, time));
                    if (dt1 < dt0)
                    {
                        bestEph = *pEph;
                    }
                }
                else
                {
                    bestEph = *pEph;
                }
            }
            if (bestEph.sat > 0)
            {
                nav->eph[pSatEph->first] = bestEph;
            }
        }
        for (std::map<int, std::vector<geph_t>>::iterator pSatEph = mGloEph.begin(); pSatEph != mGloEph.end(); ++pSatEph)
        {
            geph_t bestEph = nav->geph[pSatEph->first];
            for (std::vector<geph_t>::iterator pEph = pSatEph->second.begin(); pEph != pSatEph->second.end(); ++pEph)
            {
                dt1 = fabs(timediff(pEph->toe, time));
                if (bestEph.sat > 0)
                {
                    dt0 = fabs(timediff(bestEph.toe, time));
                    if (dt1 < dt0)
                    {
                        bestEph = *pEph;
                    }
                }
                else
                {
                    bestEph = *pEph;
                }
            }
            if (bestEph.sat > 0)
            {
                nav->geph[pSatEph->first] = bestEph;
            }
        }
        return 1;
    }
};

#define RTCM_FILE 0
#define ANDROID_PHONE 1
#define RINEX 2

struct station_t
{
    FILE* fLOG;
    rtcm_t* rtcm;
    rnxctr_t* rinex = new rnxctr_t;
    int format_;
    station_t()
    {
        fLOG = NULL;
        rtcm = new rtcm_t;
        init_rtcm(rtcm);
        format_ = RTCM_FILE;
        rinex = new rnxctr_t;
        init_rnxctr(rinex);
    }
    int open(const char* fname, int year, int mon, int day, int format)
    {
        double ep[6] = { 0 };
        if (year <= 0 || mon <= 0 || day <= 0) day = get_current_date(year, mon);
        ep[0] = year;
        ep[1] = mon;
        ep[2] = day;
        if (fLOG) fclose(fLOG); fLOG = NULL;
        if (format == RTCM_FILE)
        {
            rtcm->time = rtcm->time_s = epoch2time(ep);
            fLOG = fname ? fopen(fname, "rb") : NULL;
        }
        else if (format == ANDROID_PHONE)
        {
            fLOG = fname ? fopen(fname, "r") : NULL;
        }
        else if (format == RINEX)
        {
            fLOG = fname ? fopen(fname, "r") : NULL;
            if (fLOG && open_rnxctr(rinex, fLOG))
            {

            }
        }
        format_ = format;
        return fLOG != NULL;
    }
    ~station_t()
    {
        if (fLOG) fclose(fLOG);
        free_rtcm(rtcm);
        delete rtcm;
        free_rnxctr(rinex);
        delete rinex;
    }
    int get_obs(obsd_t* obs, double* pos)
    {
        int ret = 0;
        int data = 0;
        int nsat = 0;
        if (format_ == RTCM_FILE)
        {
            while (fLOG && !feof(fLOG) && (data = fgetc(fLOG)) != EOF)
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
        }
        else if (format_ == ANDROID_PHONE)
        {
            char buffer[1200] = { 0 };
            char* temp = nullptr;
            char* val[100];
            while (fLOG && !feof(fLOG) && (rtcm->nbyte > 0 || fgets((char*)rtcm->buff, sizeof(rtcm->buff), fLOG)))
            {
                memcpy(buffer, rtcm->buff, sizeof(rtcm->buff));
                rtcm->nbyte = 0;
                temp = strchr(buffer, '#'); if (temp) temp[0] = '\0';
                if (strlen(buffer) < 1) continue;
                int num = parse_fields(buffer, val, ',', 100);
                if (num < 1) continue;
                if (strstr(val[0], "Fix")&& num > 4)
                {
                    /*
                    # Fix,Provider,LatitudeDegrees,LongitudeDegrees,AltitudeMeters,SpeedMps,AccuracyMeters,BearingDegrees,UnixTimeMillis,SpeedAccuracyMps,BearingAccuracyDegrees,elapsedRealtimeNanos,VerticalAccuracyMeters,MockLocation,NumberOfUsedSignals,VerticalSpeedAccuracyMps,SolutionType
                    */
                    double blh[3] = { 0 };
                    blh[0] = atof(val[2]) * D2R;
                    blh[1] = atof(val[3]) * D2R;
                    blh[2] = atof(val[4]);
                    if (fabs(blh[0]) < 1e-7 && fabs(blh[1]) < 1e-7 && fabs(blh[2]) < 1e-3)
                    {
                    }
                    else
                    {
                        pos2ecef(blh, rtcm->sta.pos);
                    }
                }
                else if (strstr(val[0], "Raw") && num > 37)
                {
/*
# Raw,utcTimeMillis,TimeNanos,LeapSecond,TimeUncertaintyNanos,FullBiasNanos,BiasNanos,BiasUncertaintyNanos,DriftNanosPerSecond,DriftUncertaintyNanosPerSecond,HardwareClockDiscontinuityCount,Svid,TimeOffsetNanos,State,ReceivedSvTimeNanos,ReceivedSvTimeUncertaintyNanos,Cn0DbHz,PseudorangeRateMetersPerSecond,PseudorangeRateUncertaintyMetersPerSecond,AccumulatedDeltaRangeState,AccumulatedDeltaRangeMeters,AccumulatedDeltaRangeUncertaintyMeters,CarrierFrequencyHz,CarrierCycles,CarrierPhase,CarrierPhaseUncertainty,MultipathIndicator,SnrInDb,ConstellationType,AgcDb,BasebandCn0DbHz,FullInterSignalBiasNanos,FullInterSignalBiasUncertaintyNanos,SatelliteInterSignalBiasNanos,SatelliteInterSignalBiasUncertaintyNanos,CodeType,ChipsetElapsedRealtimeNanos,IsFullTracking
  Raw,1743793515000,19368000000,,,-1427828713632439801,0.0,46.605505803736236,,,56,6,0.0,16431,500732930425824,50,26.462682723999023,298.75151085597713,0.9404048045598317,16,6646.898744525748,340282346638528860000000000000000000000,1575420030,,,,0,,1,39.22203063964844,22.462682723999023,0.320142130558394,1.0,0.320142130558394,1.0,C,331579577538992,
*/
                    double utcTimeMillis = atof(val[1]);
                    double TimeNanos = atof(val[2]);
                    double LeapSecond = atof(val[3]);
                    double TimeUncertaintyNanos = atof(val[4]);
                    double FullBiasNanos = atof(val[5]);
                    double BiasNanos = atof(val[6]);

                    double BiasUncertaintyNanos = atof(val[7]);
                    double DriftNanosPerSecond = atof(val[8]);
                    double DriftUncertaintyNanosPerSecond = atof(val[9]);
                    double HardwareClockDiscontinuityCount = atof(val[10]);
                    int Svid = atoi(val[11]);
                    double TimeOffsetNanos = atof(val[12]);

                    int State = atoi(val[13]);
                    double ReceivedSvTimeNanos = atof(val[14]);
                    double ReceivedSvTimeUncertaintyNanos = atof(val[15]);
                    double Cn0DbHz = atof(val[16]);
                    double PseudorangeRateMetersPerSecond = atof(val[17]);
                    double PseudorangeRateUncertaintyMetersPerSecond = atof(val[18]);
                    int AccumulatedDeltaRangeState = atoi(val[19]);
                    double AccumulatedDeltaRangeMeters = atof(val[20]);
                    double AccumulatedDeltaRangeUncertaintyMeters = atof(val[21]);
                    double CarrierFrequencyHz = atof(val[22]);  /* frequency 1575420030.0 L1 1600875010.0 G1 1176450050.0 L5 1561097980.0 B1I */

                    double CarrierCycles = atof(val[23]);
                    double CarrierPhase = atof(val[24]);
                    double CarrierPhaseUncertainty = atof(val[25]);
                    double MultipathIndicator = atof(val[26]);
                    double SnrInDb = atof(val[27]);
                    int ConstellationType = atoi(val[28]);  /* 1 GPS, 2 SBS, 3 GLO, 4 QZS, 5 BDS, 6 GAL  */

                    double AgcDb = atof(val[29]);
                    double BasebandCn0DbHz = atof(val[30]);
                    double FullInterSignalBiasNanos = atof(val[31]);
                    double FullInterSignalBiasUncertaintyNanos = atof(val[32]);
                    double SatelliteInterSignalBiasNanos = atof(val[33]);
                    double SatelliteInterSignalBiasUncertaintyNanos = atof(val[34]);
                    int CodeType = atoi(val[35]); /* 1C default, STATE_GAL_E1B_PAGE_SYNC => 1B for GAL, 5Q for GPS, QZS, GAL, 2I for BDS B1I,  */
                    double ChipsetElapsedRealtimeNanos = atof(val[36]);
                    double IsFullTracking = atof(val[37]);

                    /* # Compute the GPS week number and reception time (i.e. clock epoch) */
                    int wk = (int)floor(-FullBiasNanos * 1.0e-9 / 604800);
                    double ws = (TimeNanos - (FullBiasNanos + BiasNanos)) * 1.0e-9 - wk * 604800;
                    /* # Fractional part of the integer seconds */
                    double frac = ws - int(ws + 0.5);

                    /* # Compute the reception times */
                    double trx = ws - TimeOffsetNanos * 1.0e-9;

                    /* # Compute transmit time (depends on constellation of origin) */
                    double ttx = ReceivedSvTimeNanos * 1.0e-9;

                    /* # Compute wavelength for metric conversion in cycles */
                    double wavelength = CLIGHT / CarrierFrequencyHz;

#if 0
                    int ifreq_all[] = {
                        FREQ1 / (10.23e6),            /* 1.57542E9  L1/E1/B1C  frequency (Hz) */
                        FREQ2 / (10.23e6),            /* 1.22760E9  L2         frequency (Hz) */
                        FREQ5 / (10.23e6),            /* 1.17645E9  L5/E5a/B2a frequency (Hz) */
                        FREQ6 / (10.23e6),            /* 1.27875E9  E6/L6  frequency (Hz) */
                        FREQ7 / (10.23e6),            /* 1.20714E9  E5b    frequency (Hz) */
                        FREQ8 / (10.23e6),            /* 1.191795E9 E5a+b  frequency (Hz) */
                        FREQ9 / (10.23e6),            /* 2.492028E9 S      frequency (Hz) */
                        FREQ1a_GLO / (10.23e6),       /* 1.600995E9 GLONASS G1a frequency (Hz) */
                        FREQ2a_GLO / (10.23e6),       /* 1.248060E9 GLONASS G2a frequency (Hz) */
                        FREQ1_CMP / (10.23e6),        /* 1.561098E9 BDS B1I     frequency (Hz) */
                        FREQ2_CMP / (10.23e6),        /* 1.20714E9  BDS B2I/B2b frequency (Hz) */
                        FREQ3_CMP / (10.23e6),        /* 1.26852E9  BDS B3      frequency (Hz) */
                        FREQ1_GLO / (10.23e6),        /* 1.60200E9  GLONASS G1 base frequency (Hz) */
                        FREQ2_GLO / (10.23e6),        /* 1.24600E9  GLONASS G2 base frequency (Hz) */
                    };
#endif

                    int sys = 0;
                    int prn = Svid;
                    int sat = 0;
                    int code = 0;
                    int f = 0;
                    int ifreq = (int)(CarrierFrequencyHz / (10.23e6));
                    if (ConstellationType == 1)
                    {
                        sys = SYS_GPS;
                        if (ifreq == 154)
                            code = CODE_L1C;
                        else if (ifreq == 115)
                            code = CODE_L5Q;
                    }
                    else if (ConstellationType == 2)
                    {
                        sys = SYS_SBS;
                    }
                    else if (ConstellationType == 3)
                    {
                        sys = SYS_GLO;
                        if (ifreq >= 154)
                            code = CODE_L1C;
                        ttx += - 3 * 3600 + 18.0; /* conver to GPS time */
                        int wday = (int)floor((trx - ttx) / (3600.0 * 24) + 0.5);
                        ttx += wday * (3600.0 * 24.0);
                    }
                    else if (ConstellationType == 4)
                    {
                        sys = SYS_QZS;
                        if (ifreq == 154)
                            code = CODE_L1C;
                        else if (ifreq == 115)
                            code = CODE_L5Q;
                    }
                    else if (ConstellationType == 5)
                    {
                        sys = SYS_CMP;
                        ttx += 14.0;
                        code = CODE_L2I;
                    }
                    else if (ConstellationType == 6)
                    {
                        sys = SYS_GAL;
                        if (ifreq == 154)
                            code = CODE_L1C;
                        else if (ifreq == 115)
                            code = CODE_L5Q;
                    }
                    else
                    {

                    }

                    gtime_t obs_time = gpst2time(wk, ws);
#ifdef _DEBUG
                    printf("%s,%4i,%20.12f,%3i,%3i,%3i,%3i", time_str(obs_time,3), wk, ws, ifreq, sys, prn, code);
#endif
                    if (sys > 0 && (sat = satno(sys, prn)) > 0 && (f = code2idx(sys, code)) >= 0 && f < (NFREQ + NEXOBS))
                    {

                        double tau = trx - ttx;
                        if (tau > (24 * 7 * 1800)) tau -= 24 * 7 * 3600;
                        else if (tau < -(24 * 7 * 1800)) tau += 24 * 7 * 3600;

                        //double dt = (tRxSeconds * 1e7 - floor(tRxSeconds * 1e7)) * 1.0e-7; /* it seems that the rinex from GNSSLogger directly compensate the dist caused by the round of 1.0e-7s */
                        //double dist = dt * CLIGHT;
                        double range = tau * CLIGHT;
                        //range += dist; /* match the rinex output */

                        //range -= frac * PseudorangeRateMetersPerSecond;

                        double phase = AccumulatedDeltaRangeMeters / wavelength;

                        double doppler = -PseudorangeRateMetersPerSecond / wavelength;

                        double snr = Cn0DbHz;

                        //ws -= frac;


                        obsd_t cur_obs = { 0 };
                        cur_obs.time = obs_time;
                        cur_obs.sat = satno(sys, prn);
                        cur_obs.code[f] = code;
                        cur_obs.P[f] = range;
                        cur_obs.L[f] = phase;
                        cur_obs.D[f] = (float)doppler;
                        cur_obs.SNR[f] = (uint16_t)(snr / SNR_UNIT);
#ifdef _DEBUG
                        printf(",%14.4f,%14.4f,%10.4f,%7.3f\n", range, phase, doppler, snr);
#endif
                        if (rtcm->obs.n > 0)
                        {
                            obsd_t* obs_pre = rtcm->obs.data + (rtcm->obs.n - 1);
                            double dt1 = timediff(obs_time, obs_pre->time);
                            if (fabs(dt1) < 0.001) /* same epoch */
                            {
                                if (rtcm->obs.n < MAXOBS)
                                {
                                    rtcm->obs.data[rtcm->obs.n++] = cur_obs;
                                }
                            }
                            else
                            {
                                rtcm->nbyte = (int)strlen((char*)rtcm->buff);
                                /* new epoch, store the current buffer */
                                ret = 1;
                                for (int i = 0; i < rtcm->obs.n; ++i)
                                {
                                    obs[nsat] = rtcm->obs.data[i];
                                    ++nsat;
                                }
                                pos[0] = rtcm->sta.pos[0];
                                pos[1] = rtcm->sta.pos[1];
                                pos[2] = rtcm->sta.pos[2];
                                rtcm->obs.n = 0;
                                break;
                            }
                        }
                        else
                        {
                            rtcm->obs.data[rtcm->obs.n++] = cur_obs;
                        }
                    }
                    else
                    {
#ifdef _DEBUG
                        printf("\n");
#endif
                    }
                }
            }
        }
        else if (format_ == RINEX)
        {
            while (fLOG && !feof(fLOG))
            {
                ret = input_rnxctr(rinex, fLOG);
                if (ret < 0) break; /* end of file */
                if (ret == 1)
                {
                    for (int i = 0; i < rinex->obs.n; ++i)
                    {
                        obs[nsat] = rinex->obs.data[i];
                        ++nsat;
                    }
                    pos[0] = rinex->sta.pos[0];
                    pos[1] = rinex->sta.pos[1];
                    pos[2] = rinex->sta.pos[2];
                    break;
                }
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
                coord.coord_system_name = coord_name_regional;
                coord.epoch = epoch_regional;
                coord.amb_fix_rate = amb_fix_rate;
                coord.xyz[0] = xyz_regional[0];
                coord.xyz[1] = xyz_regional[1];
                coord.xyz[2] = xyz_regional[2];
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

static int read_ccsv_file(const char* fname, std::map<std::string, coord_t>& mCoords)
{
    int ret = 0;
    FILE* fCCSV = fopen(fname, "r");
    char buffer[512] = { 0 };
    char* temp = nullptr;
    char* val[100];

    while (fCCSV && !feof(fCCSV) && fgets(buffer, sizeof(buffer), fCCSV))
    {
        /* Station,Date,Platform,Status,Result,Message,Rejected Epochs,Fixed Ambiguities,Sigmas(95%) North,SigmasE(95%) East,SigmasU(95%) Height,Primitive,ITRF2020,WGS84,Regional       */
        if (strstr(buffer, "Station") && strstr(buffer, "Date")) continue;
        if (temp = strchr(buffer, '\n')) temp[0] = '\0';
        while (temp = strchr(buffer, ':')) temp[0] = ',';
        while (temp = strchr(buffer, ';')) temp[0] = ',';
        int num = parse_fields(buffer, val, ',', 100);
        if (num < 51) continue;
        coord_t coord;
        double blh[3] = { 0 };
        coord.name = std::string(val[0]);
        coord.amb_fix_rate = atof(val[7]);
        double sigmaN = atof(val[8]);
        double sigmaE = atof(val[9]);
        double sigmaU = atof(val[10]);
        coord.coord_system_name = std::string(val[12]);
        coord.epoch = atof(val[14]);
        coord.xyz[0] = atof(val[16]);
        coord.xyz[1] = atof(val[18]);
        coord.xyz[2] = atof(val[20]);
        ecef2pos(coord.xyz, blh);

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

        /* dXYZ = C_en*dNED */

        /* cov(xyz) = C_en*cov(ned)*C_en' */
        double covX = C_en[0][0] * sigmaN * sigmaN * C_en[0][0] + C_en[0][1] * sigmaE * sigmaE * C_en[0][1] + C_en[0][2] * sigmaU * sigmaU * C_en[0][2];
        double covY = C_en[1][0] * sigmaN * sigmaN * C_en[1][0] + C_en[1][1] * sigmaE * sigmaE * C_en[1][1] + C_en[1][2] * sigmaU * sigmaU * C_en[1][2];
        double covZ = C_en[2][0] * sigmaN * sigmaN * C_en[2][0] + C_en[2][1] * sigmaE * sigmaE * C_en[2][1] + C_en[2][2] * sigmaU * sigmaU * C_en[2][2];
        coord.sigma95_xyz[0] = sqrt(covX);
        coord.sigma95_xyz[1] = sqrt(covY);
        coord.sigma95_xyz[2] = sqrt(covZ);
        if (mCoords[coord.name].epoch == 0 || mCoords[coord.name].epoch < coord.epoch)
        {
            mCoords[coord.name] = coord;
        }
    }
    if (fCCSV) fclose(fCCSV);
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

typedef struct
{
    int sat;
    int sys;
    int prn;
    int svh;
    double ws;
    double var;
    double rs[6];
    double dt[2];
}svec_t;

extern int comp_vec(obsd_t* obs, svec_t* vec, int n, nav_t* nav)
{
    int ret = 0;
    double* rs, * dts, * var;
    int i, * svh;
    int wk = 0;
    if (n > 0)
    {
        rs = mat(6, n); dts = mat(2, n); var = mat(1, n);
        svh = new int[n];

        /* satellite positons, velocities and clocks */
        satposs(obs[0].time, obs, n, nav, EPHOPT_BRDC, rs, dts, var, svh);

        for (i = 0; i < n; ++i)
        {
            memset(vec + i, 0, sizeof(svec_t));
            vec[i].sat = obs[i].sat;
            vec[i].sys = satsys(vec[i].sat, &vec[i].prn);
            vec[i].ws = time2gpst(obs[i].time, &wk);
            vec[i].svh = svh[i];
            vec[i].var = var[i];
            vec[i].rs[0] = rs[i * 6 + 0];
            vec[i].rs[1] = rs[i * 6 + 1];
            vec[i].rs[2] = rs[i * 6 + 2];
            vec[i].rs[3] = rs[i * 6 + 3];
            vec[i].rs[4] = rs[i * 6 + 4];
            vec[i].rs[5] = rs[i * 6 + 5];
            vec[i].dt[0] = dts[i * 2 + 0] * CLIGHT;
            vec[i].dt[1] = dts[i * 2 + 1] * CLIGHT;
            if (!vec[i].svh && (fabs(vec[i].rs[0]) < 0.001 || fabs(vec[i].rs[1]) < 0.001 || fabs(vec[i].rs[2]) < 0.001)) vec[i].svh = 255;
        }

        free(rs); free(dts); free(var);
        delete[]svh;
    }

    return ret;
}

static int output_data(int rcv, obsd_t* obs, int n, double* pos, nav_t* nav, FILE* fOBS)
{
    int ret = 0;
    int wk = 0;
    int i = 0;
    double ws = 0;
    svec_t* vec = nullptr;

    if (n > 0)
    {
        vec = new svec_t[n];

        comp_vec(obs, vec, n, nav);

        if (fOBS)
        {
            ws = time2gpst(obs[0].time, &wk);
            fprintf(fOBS, "POS,%04i,%10.4f,%i,%14.4f,%14.4f,%14.4f\r\n", wk, ws, rcv, pos[0], pos[1], pos[2]);
            for (i = 0; i < n; ++i)
            {
                ws = time2gpst(obs[i].time, &wk);
                for (int f = 0; f < NFREQ + NEXOBS; ++f)
                {
                    if (obs[i].code[f] == 0) continue;
                    fprintf(fOBS, "OBS,%04i,%10.4f,%i,%3i,%3i,%14.4f,%14.4f,%10.4f,%3i,%lf\r\n", wk, ws, rcv, obs[i].sat, obs[i].code[f], obs[i].P[f], obs[i].L[f], obs[i].D[f], (int)(obs[i].SNR[f] * SNR_UNIT), sat2freq(obs[i].sat, obs[i].code[f], nav));
                }
            }
            for (i = 0; i < n; ++i)
            {
                ws = time2gpst(obs[i].time, &wk);
                int prn = 0;
                int sys = satsys(vec[i].sat, &prn);
                fprintf(fOBS, "VEC,%04i,%10.4f,%i,%3i,%3i,%3i,%14.4f,%14.4f,%14.4f,%10.4f,%10.4f,%10.4f,%14.4f,%10.4f,%10.4f,%i\r\n", wk, ws, rcv, vec[i].sat, sys, prn, vec[i].rs[0], vec[i].rs[1], vec[i].rs[2], vec[i].rs[3], vec[i].rs[4], vec[i].rs[5], vec[i].dt[0], vec[i].dt[1], vec[i].var, vec[i].svh);
            }
        }

        delete[] vec;
    }
    return ret;
}

static int process_log(const char* rovefname, const char* basefname, const char* brdcfname, int year, int mm, int dd, double *pos_base_external, int format, int opt)
{
    int ret = 0;
    artk_t* artk = new artk_t;

    brdc_t* brdc = new brdc_t;
    brdc->read_rtcm_data(basefname, year, mm, dd);
    brdc->read_rtcm_data(brdcfname, year, mm, dd);

    station_t* base = new station_t;
    station_t* rove = new station_t;

    rove->open(rovefname, year, mm, dd, format);
    base->open(basefname, year, mm, dd, RTCM_FILE);

    obsd_t* obsd = new obsd_t[MAXOBS + MAXOBS];
    memset(obsd, 0, sizeof(obsd_t) * (MAXOBS + MAXOBS));
    int nsat[2] = { 0 };
    double pos[12] = { 0 };
    obsd_t* obs_rove = obsd;
    obsd_t* obs_base = obsd + MAXOBS;
    double* pos_rove = pos + 0; /* current rove position */
    double* pos_base = pos + 3; /* current base position */
    double* pos_rove0 = pos + 6;/* initial rove position */
    double* pos_base0 = pos + 9;/* initial base position */
    double dxyz[4] = { 0 };

    FILE* fGGA = (opt & 1 << 1) ? NULL : set_output_file2(rovefname, basefname, ".nmea");
    FILE* fSOL = (opt & 1 << 2) ? NULL : set_output_file2(rovefname, basefname, ".pos");
    FILE* fOBS = (opt & 1 << 2) ? NULL : set_output_file2(rovefname, basefname, ".obs");

    double dt = 0;

    int count = 0;
    gtime_t stime = { 0 };
    gtime_t etime = { 0 };

    while (true)
    {
        nsat[0] = rove->get_obs(obs_rove, pos_rove);
        if (!nsat[0]) break;
        if (count)
            etime = obs_rove[0].time;
        else
            stime = obs_rove[0].time;
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
                printf("#base coordinate rtcm initial %s %14.9f %14.9f %14.4f %14.4f %14.4f %14.4f\r\n", time_str(obs_base[0].time, 3), blh[0] * R2D, blh[1] * R2D, blh[2], pos_base0[0], pos_base0[1], pos_base0[2]);
            }
            else
            {
                /* check changes */
                dxyz[0] = pos_base[0] - pos_base0[0];
                dxyz[1] = pos_base[1] - pos_base0[1];
                dxyz[2] = pos_base[2] - pos_base0[2];
                dxyz[3] = sqrt(dxyz[0] * dxyz[0] + dxyz[1] * dxyz[1] + dxyz[2] * dxyz[2]);
                if (dxyz[3] > 10.0)
                {
                    double blh[3] = { 0 };
                    ecef2pos(pos_base, blh);
                    printf("#base rtcm coordinate changed %s %14.9f %14.9f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f %14.4f\r\n", time_str(obs_rove[0].time, 3), blh[0] * R2D, blh[1] * R2D, blh[2], pos_base0[0], pos_base0[1], pos_base0[2], dxyz[0], dxyz[1], dxyz[2], dxyz[3]);
                    pos_base0[0] = pos_base[0];
                    pos_base0[1] = pos_base[1];
                    pos_base0[2] = pos_base[2];
                }
            }
            if (fabs(pos_base0[0]) < 0.001 || fabs(pos_base0[1]) < 0.001 || fabs(pos_base0[2]) < 0.001)
            {
                if (fabs(pos_base_external[0]) < 0.001 || fabs(pos_base_external[1]) < 0.001 || fabs(pos_base_external[2]) < 0.001)
                {
                }
                else
                {
                    pos_base0[0] = pos_base_external[0];
                    pos_base0[1] = pos_base_external[1];
                    pos_base0[2] = pos_base_external[2];
                    double blh[3] = { 0 };
                    ecef2pos(pos_base0, blh);
                    printf("#base coordinate external     %s %14.9f %14.9f %14.4f %14.4f %14.4f %14.4f\r\n", time_str(obs_base[0].time, 3), blh[0] * R2D, blh[1] * R2D, blh[2], pos_base0[0], pos_base0[1], pos_base0[2]);
                }
            }
        }
        /* base and rove data ready */
        if (nsat[0] > 0 && artk->add_rove_obs(obs_rove, nsat[0], pos_rove0))
        {
            brdc->get_brdc_data(obs_rove[0].time, &rove->rtcm->nav);
            artk->add_brdc(&rove->rtcm->nav);
            /* output to file */
            output_data(1, obs_rove, nsat[0], pos_rove0, &rove->rtcm->nav, fOBS);
            output_data(2, obs_base, nsat[1], pos_base0, &rove->rtcm->nav, fOBS);
            /* call RTK engine */
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
            }
            count++;
        }
    }

    delete brdc;
    delete base;
    delete rove;
    delete[] obsd;

    delete artk;

    if (!(opt & (1 << 0))) printf("beg =%s\n", time_str(stime,3));
    if (!(opt & (1 << 0))) printf("end =%s\n", time_str(etime, 3));
    if (!(opt & (1 << 0))) printf("rove=%s\n", rovefname);
    if (!(opt & (1 << 0))) printf("base=%s\n", basefname);
    if (!(opt & (1 << 0))) printf("brdc=%s\n", brdcfname);

    if (fSOL) fclose(fSOL);
    if (fGGA) fclose(fGGA);
    if (fOBS) fclose(fOBS);

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
        int opt = 0;
        int format = RTCM_FILE;
        std::string rovefname;
        std::string basefname;
        std::string brdcfname;
        std::map<std::string, coord_t> mCoords;
        std::string jsonfname;
        std::string ccsvfname;
        char* temp = nullptr;
        double pos_base[3] = { 0 };
        for (int i = 1; i < argc; ++i)
        {
            if (temp = strchr(argv[i], '='))
            {
                temp[0] = '\0';
                if (strstr(argv[i], "rove"))
                {
                    if (strstr(argv[i], "android"))
                        format = ANDROID_PHONE;
                    else if (strstr(argv[i], "rinex"))
                        format = RINEX;
                    else if (strstr(argv[i], "rtcm"))
                        format = RTCM_FILE;
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
                else if (strstr(argv[i], "ccsv"))
                {
                    ccsvfname = std::string(temp + 1);
                    read_ccsv_file(ccsvfname.c_str(), mCoords);
                }
                else if (strstr(argv[i], "brdc"))
                {
                    brdcfname = std::string(temp + 1);
                }
                else if (strstr(argv[i], "opt"))
                {
                    int opt_bit = atoi(temp + 1);
                    if (opt_bit >= 0 && opt_bit < 8)
                    {
                        /* 0 => disable screen */
                        /* 1 => disable NMEA GGA */
                        /* 2 => disable CSV SOL */
                        opt |= 1 << opt_bit;
                    }
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
                else if (strstr(argv[i], "pos0"))
                {
                    char* val[100];
                    int num = parse_fields(temp + 1, val, ',', 100);
                    if (num > 2)
                    {
                        pos_base[0] = atof(val[0]);
                        pos_base[1] = atof(val[1]);
                        pos_base[2] = atof(val[2]);
                    }
                }
            }

        }
        clock_t stime = clock();

        for (std::map<std::string, coord_t>::iterator pCoord = mCoords.begin(); pCoord != mCoords.end(); ++pCoord)
        {
            if (basefname.find(pCoord->first) != std::string::npos)
            {
                pos_base[0] = pCoord->second.xyz[0];
                pos_base[1] = pCoord->second.xyz[1];
                pos_base[2] = pCoord->second.xyz[2];
            }
        }

        ret = process_log(rovefname.c_str(), basefname.c_str(), brdcfname.c_str(), yyyy, mm, dd, pos_base, format, opt);

        clock_t etime = clock();
        double cpu_time_used = ((double)(etime - stime)) / CLOCKS_PER_SEC;
        if (!(opt & 1 << 0)) printf("time=%.3f[s]\n", cpu_time_used);
    }
    return ret;
}
