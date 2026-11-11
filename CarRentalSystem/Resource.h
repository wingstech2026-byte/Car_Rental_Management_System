// =============================================================================
// Resource.h - identifiers shared by CarRental.rc and the C++ code.
// =============================================================================
#ifndef CAR_RENTAL_RESOURCE_H
#define CAR_RENTAL_RESOURCE_H

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

// ---- Menu and accelerators ---------------------------------------------------
#define IDR_MAINMENU            101
#define IDR_ACCELERATORS        102

// ---- Dialog templates ----------------------------------------------------------
#define IDD_VEHICLE             201
#define IDD_RENT                202
#define IDD_RETURN              203
#define IDD_LOGIN               204
#define IDD_USERS               205
#define IDD_CHANGE_PASSWORD     206

// ---- Add/Edit Vehicle dialog ---------------------------------------------------
#define IDC_PLATE_EDIT          1001
#define IDC_MODEL_EDIT          1002
#define IDC_RATE_EDIT           1003
#define IDC_PLATE_HINT          1004

// ---- Rent dialog -------------------------------------------------------------
#define IDC_RENT_PLATE          1101
#define IDC_RENT_MODEL          1102
#define IDC_RENT_CURRENT_RATE   1103
#define IDC_RENTER_NAME         1104
#define IDC_RENTER_PHONE        1105
#define IDC_AGREED_RATE         1106

// ---- Return dialog -------------------------------------------------------------
#define IDC_RET_PLATE           1201
#define IDC_RET_MODEL           1202
#define IDC_RET_RENTER          1203
#define IDC_RET_PHONE           1204
#define IDC_RET_RATE            1205
#define IDC_DAYS_EDIT           1206
#define IDC_DAYS_SPIN           1207
#define IDC_CHARGE_PREVIEW      1208
#define IDC_RET_START           1209

// ---- Login dialog ----------------------------------------------------------------
#define IDC_LOGIN_USERNAME      1301
#define IDC_LOGIN_PASSWORD      1302
#define IDC_LOGIN_INFO          1303

// ---- Users dialog ----------------------------------------------------------------
#define IDC_USERS_LIST          1401
#define IDC_USERS_NAME          1402
#define IDC_USERS_PASSWORD      1403
#define IDC_USERS_ROLE          1404
#define IDC_USERS_ADD           1405
#define IDC_USERS_REMOVE        1406
#define IDC_USERS_RESET         1407

// ---- Change password dialog ------------------------------------------------------
#define IDC_PW_CURRENT          1501
#define IDC_PW_NEW              1502
#define IDC_PW_CONFIRM          1503

// ---- Main window controls ------------------------------------------------------
#define IDC_TAB                 2001
#define IDC_FLEET_HINT          2002
#define IDC_FLEET_LIST          2003
#define IDC_ADD_BUTTON          2004
#define IDC_EDIT_BUTTON         2005
#define IDC_SEARCH_LABEL        2006
#define IDC_SEARCH_EDIT         2007
#define IDC_SEARCH_LIST         2008
#define IDC_SEARCH_COUNT        2009
#define IDC_RENT_BUTTON         2010
#define IDC_REVENUE_LABEL       2011
#define IDC_RENTALS_LIST        2012
#define IDC_RENTALS_HINT        2013
#define IDC_RETURN_BUTTON       2014
#define IDC_STATUS_BAR          2015
#define IDC_HISTORY_SUMMARY     2016
#define IDC_HISTORY_LABEL       2017
#define IDC_HISTORY_SEARCH      2018
#define IDC_HISTORY_LIST        2019
#define IDC_HISTORY_HINT        2020
#define IDC_FLEET_SEARCH_LABEL  2021
#define IDC_FLEET_SEARCH        2022
#define IDC_FLEET_FILTER        2023
#define IDC_REMOVE_BUTTON       2024
#define IDC_REPORTS_SUMMARY     2025
#define IDC_MODEL_LABEL         2026
#define IDC_MODEL_LIST          2027
#define IDC_TOPVEHICLE_LABEL    2028
#define IDC_TOPVEHICLE_LIST     2029
#define IDC_PAGE_FIRST          2100

// ---- Menu / accelerator commands -----------------------------------------------
#define ID_FILE_SAVE            40001
#define ID_FILE_EXIT            40002
#define ID_HELP_ABOUT           40003
#define ID_HELP_SELFTEST        40004
#define ID_NEXT_TAB             40005
#define ID_PREV_TAB             40006
#define ID_FIND                 40007
#define ID_FILE_RELOAD          40008
#define ID_FILE_LOGOUT          40009
#define ID_USERS_MANAGE         40010
#define ID_USERS_PASSWORD       40011

#endif // CAR_RENTAL_RESOURCE_H
