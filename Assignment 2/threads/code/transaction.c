#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <time.h>
#include <sys/time.h>
#include <inttypes.h>

#include "bank.h"
#include "account.h"
#include "branch.h"
#include "teller.h"
#include "action.h"
#include "report.h"
#include "error.h"
#include "transaction.h"

#include "debug.h"



/*
 * Perform transaction. Returns -1 if done otherwise returns error code. 
 */

int
Transaction_DoAction(Bank *bank, Action *actionPtr, int workerNum, 
		    AccountAmount fixedBankBalance)
{
    int err;
    AccountAmount balance;

    switch (actionPtr->cmd) {
    case ACTION_DONE:
      err = -1;
      break;
    case ACTION_DEPOSIT:
      err = Teller_DoDeposit(bank, actionPtr->u.depwithArg.accountNum,
                             actionPtr->u.depwithArg.amount);
      if (err == ERROR_SUCCESS) {
        Report_Transfer(bank, workerNum,  actionPtr->u.depwithArg.accountNum,
                        actionPtr->u.depwithArg.amount);
      }

      break;
    case ACTION_WITHDRAW:
      err = Teller_DoWithdraw(bank, actionPtr->u.depwithArg.accountNum,
                              actionPtr->u.depwithArg.amount);
      if (err == ERROR_SUCCESS) {
        Report_Transfer(bank, workerNum,  actionPtr->u.depwithArg.accountNum,
                        -actionPtr->u.depwithArg.amount);
      }
      break;
    case ACTION_TRANSFER:
      err = Teller_DoTransfer(bank,
                              actionPtr->u.transArg.srcAccountNum,
                              actionPtr->u.transArg.dstAccountNum,
                              actionPtr->u.transArg.amount);
      if (err == ERROR_SUCCESS) {
        Report_Transfer(bank, workerNum,  actionPtr->u.transArg.srcAccountNum,
                        -actionPtr->u.transArg.amount);
        Report_Transfer(bank, workerNum,  actionPtr->u.transArg.dstAccountNum,
                        actionPtr->u.transArg.amount);
      }
      break;
    case ACTION_BRANCH_BALANCE:
      err = Branch_Balance(bank,actionPtr->u.branchArg.branchID, &balance);
      DPRINTF('b', ("Branch %"PRIu64" balance is %"PRId64"\n",
                    actionPtr->u.branchArg.branchID, balance));
      break;
    case ACTION_BANK_BALANCE:
      err = Bank_Balance(bank, &balance);
      DPRINTF('b', ("Bank balance is %"PRId64"\n", balance));
      if (fixedBankBalance && (balance != fixedBankBalance)) {
	err = ERROR_BALANCE_MISMATCH;
        fprintf(stderr, "Bank balance incorrect (%"PRId64" != %"PRId64")\n", 
                balance, fixedBankBalance);
      }
      break;
    case ACTION_REPORT:
      err = Report_DoReport(bank, actionPtr->u.reportArg.workerNum);
      if (err != 0) {
        fprintf(stderr, "Report_DoReport(Worker=%d) returns %d\n",
                actionPtr->u.reportArg.workerNum,
                err);
        err = 0; // Mask error so we don't abort on a report error
      }
      break;
    default:
      fprintf(stderr, "Unknown action cmd %d\n", actionPtr->cmd);
      err = -1;
      break;
    }
    return err;
}
