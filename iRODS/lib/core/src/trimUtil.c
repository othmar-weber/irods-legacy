/*** Copyright (c), The Regents of the University of California            ***
 *** For more information please refer to files in the COPYRIGHT directory ***/
#ifndef windows_platform
#include <sys/time.h>
#endif
#include "rodsPath.h"
#include "rodsErrorTable.h"
#include "rodsLog.h"
#include "miscUtil.h"
#include "trimUtil.h"

int
trimUtil (rcComm_t *conn, rodsEnv *myRodsEnv, rodsArguments_t *myRodsArgs,
rodsPathInp_t *rodsPathInp)
{
    int i;
    int status; 
    int savedStatus = 0;
    dataObjInp_t dataObjInp;


    if (rodsPathInp == NULL) {
	return (USER__NULL_INPUT_ERR);
    }

    initCondForTrim (myRodsEnv, myRodsArgs, &dataObjInp);

    for (i = 0; i < rodsPathInp->numSrc; i++) {
	if (rodsPathInp->srcPath[i].objType == UNKNOWN_OBJ_T) {
	    getRodsObjType (conn, &rodsPathInp->srcPath[i]);
	    if (rodsPathInp->srcPath[i].objState == NOT_EXIST_ST) {
                rodsLog (LOG_ERROR,
                  "trimUtil: srcPath %s does not exist",
                  rodsPathInp->srcPath[i].outPath);
		savedStatus = USER_INPUT_PATH_ERR;
		continue;
	    }
	}

	if (rodsPathInp->srcPath[i].objType == DATA_OBJ_T) {
	    rmKeyVal (&dataObjInp.condInput, TRANSLATED_PATH_KW);
	    status = trimDataObjUtil (conn, rodsPathInp->srcPath[i].outPath, 
	     myRodsEnv, myRodsArgs, &dataObjInp);
	} else if (rodsPathInp->srcPath[i].objType ==  COLL_OBJ_T) {
	    addKeyVal (&dataObjInp.condInput, TRANSLATED_PATH_KW, "");
	    status = trimCollUtil (conn, rodsPathInp->srcPath[i].outPath,
              myRodsEnv, myRodsArgs, &dataObjInp);
	} else {
	    /* should not be here */
	    rodsLog (LOG_ERROR,
	     "trimUtil: invalid trim objType %d for %s", 
	     rodsPathInp->srcPath[i].objType, rodsPathInp->srcPath[i].outPath);
	    return (USER_INPUT_PATH_ERR);
	}
	/* XXXX may need to return a global status */
	if (status < 0) {
	    rodsLogError (LOG_ERROR, status,
             "trimUtil: trim error for %s, status = %d", 
	      rodsPathInp->srcPath[i].outPath, status);
	    savedStatus = status;
	} 
    }
    if (savedStatus < 0) {
        return (savedStatus);
    } else if (status == CAT_NO_ROWS_FOUND) {
        return (0);
    } else {
        return (status);
    }
}

int
trimDataObjUtil (rcComm_t *conn, char *srcPath, 
rodsEnv *myRodsEnv, rodsArguments_t *rodsArgs, 
dataObjInp_t *dataObjInp)
{
    int status;
    struct timeval startTime, endTime;
 
    if (srcPath == NULL) {
       rodsLog (LOG_ERROR,
          "trimDataObjUtil: NULL srcPath input");
        return (USER__NULL_INPUT_ERR);
    }

    if (rodsArgs->verbose == True) {
        (void) gettimeofday(&startTime, (struct timezone *)0);
    }

    rstrcpy (dataObjInp->objPath, srcPath, MAX_NAME_LEN);

    status = rcDataObjTrim (conn, dataObjInp);

    if (status >= 0 && rodsArgs->verbose == True) {
        (void) gettimeofday(&endTime, (struct timezone *)0);
        printTiming (conn, dataObjInp->objPath, conn->transStat.bytesWritten, 
	  NULL, &startTime, &endTime);
    }

    return (status);
}

int
initCondForTrim (rodsEnv *myRodsEnv, rodsArguments_t *rodsArgs, 
dataObjInp_t *dataObjInp)
{
    char tmpStr[NAME_LEN];

    if (dataObjInp == NULL) {
       rodsLog (LOG_ERROR,
          "initCondForTrim: NULL dataObjInp input");
        return (USER__NULL_INPUT_ERR);
    }

    memset (dataObjInp, 0, sizeof (dataObjInp_t));

    if (rodsArgs == NULL) {
	return (0);
    }

    if (rodsArgs->number == True) {
	snprintf (tmpStr, NAME_LEN, "%d", rodsArgs->numberValue);
        addKeyVal (&dataObjInp->condInput, COPIES_KW, tmpStr);
    }

    if (rodsArgs->admin == True) {
        addKeyVal (&dataObjInp->condInput, IRODS_ADMIN_KW, "");
    }

    if (rodsArgs->replNum == True) {
        addKeyVal (&dataObjInp->condInput, REPL_NUM_KW, 
	  rodsArgs->replNumValue);
    }

    if (rodsArgs->srcResc == True) {
        addKeyVal (&dataObjInp->condInput, RESC_NAME_KW,
          rodsArgs->srcRescString);
    }

    return (0);
}

int
trimCollUtil (rcComm_t *conn, char *srcColl, rodsEnv *myRodsEnv, 
rodsArguments_t *rodsArgs, dataObjInp_t *dataObjInp)
{
    int status;
    int savedStatus = 0;
    char srcChildPath[MAX_NAME_LEN];
#if 0
    int collLen;
#endif
    collHandle_t collHandle;
    collEnt_t collEnt;

    if (srcColl == NULL) {
       rodsLog (LOG_ERROR,
          "trimCollUtil: NULL srcColl input");
        return (USER__NULL_INPUT_ERR);
    }

    if (rodsArgs->recursive != True) {
        rodsLog (LOG_ERROR,
        "trimCollUtil: -r option must be used for getting %s collection",
         srcColl);
        return (USER_INPUT_OPTION_ERR);
    }

    if (rodsArgs->verbose == True) {
        fprintf (stdout, "C- %s:\n", srcColl);
    }

#if 0
    status = rclOpenCollection (conn, srcColl, RECUR_QUERY_FG,
      &collHandle);
#else
    status = rclOpenCollection (conn, srcColl, 0, &collHandle);
#endif

    if (status < 0) {
        rodsLog (LOG_ERROR,
          "trimCollUtil: rclOpenCollection of %s error. status = %d",
          srcColl, status);
        return status;
    }
#if 0
    collLen = strlen (srcColl);
    collLen = getOpenedCollLen (&collHandle);
#else
    if (collHandle.rodsObjStat->specColl != NULL && 
      collHandle.rodsObjStat->specColl->collClass != LINKED_COLL) {
	/* no trim for mounted coll */
	rclCloseCollection (&collHandle);
	return 0;
    }
#endif
    while ((status = rclReadCollection (conn, &collHandle, &collEnt)) >= 0) {
        if (collEnt.objType == DATA_OBJ_T) {
            snprintf (srcChildPath, MAX_NAME_LEN, "%s/%s",
              collEnt.collName, collEnt.dataName);
            status = trimDataObjUtil (conn, srcChildPath, myRodsEnv, rodsArgs,
              dataObjInp);
            if (status < 0) {
                rodsLogError (LOG_ERROR, status,
                  "trimCollUtil: trimDataObjUtil failed for %s. status = %d",
                  srcChildPath, status);
                /* need to set global error here */
                savedStatus = status;
                status = 0;
            }
        } else if (collEnt.objType == COLL_OBJ_T) {
            dataObjInp_t childDataObjInp;
            childDataObjInp = *dataObjInp;
	    if (collEnt.specColl.collClass != NO_SPEC_COLL)
                childDataObjInp.specColl = &collEnt.specColl;
            else 
                childDataObjInp.specColl = NULL;
            status = trimCollUtil (conn, collEnt.collName, myRodsEnv, 
	      rodsArgs, &childDataObjInp);
            if (status < 0 && status != CAT_NO_ROWS_FOUND) {
                return (status);
            }
        }
    }
    rclCloseCollection (&collHandle);

    if (savedStatus < 0) {
	return (savedStatus);
    } else if (status == CAT_NO_ROWS_FOUND) {
	return (0);
    } else {
        return (status);
    }
}

