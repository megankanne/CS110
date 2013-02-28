#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include "teller.h"
#include "account.h"
#include "error.h"
#include "debug.h"
#include "branch.h"

/*
 * deposit money into an account
 */
int
Teller_DoDeposit(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
	assert(amount >= 0);

	DPRINTF('t', ("Teller_DoDeposit(account 0x%"PRIx64" amount %"PRId64")\n",
		          accountNum, amount));

	Account *account = Account_LookupByNumber(bank, accountNum);

	if (account == NULL) {
	 return ERROR_ACCOUNT_NOT_FOUND;
	}

	// Initialize and lock account
	pthread_mutex_init(&(account->accountLock), NULL);
	pthread_mutex_lock(&(account->accountLock));

	// Find the branch the account is contained in
	uint64_t branchID = AccountNum_GetBranchID(account->accountNumber);
	Branch branch = bank->branches[branchID];

	// Initialize and lock branch
	pthread_mutex_init(&(branch.branchLock), NULL);
	pthread_mutex_lock(&(branch.branchLock));

	Account_Adjust(bank, account, amount, 1);

	// Unlock branch and account
	pthread_mutex_unlock(&(account->accountLock));
	pthread_mutex_unlock(&(branch.branchLock));

	return ERROR_SUCCESS;
}

/*
 * withdraw money from an account
 */
int
Teller_DoWithdraw(Bank *bank, AccountNumber accountNum, AccountAmount amount)
{
	assert(amount >= 0);

	DPRINTF('t', ("Teller_DoWithdraw(account 0x%"PRIx64" amount %"PRId64")\n",
		          accountNum, amount));

	Account *account = Account_LookupByNumber(bank, accountNum);

	if (account == NULL) {
	 return ERROR_ACCOUNT_NOT_FOUND;
	}

	if (amount > Account_Balance(account)) {
	 return ERROR_INSUFFICIENT_FUNDS;
	}

	// Initialize and lock account
	pthread_mutex_init(&(account->accountLock), NULL);
	pthread_mutex_lock(&(account->accountLock));

	// Find the branch the account is contained in
	uint64_t branchID = AccountNum_GetBranchID(account->accountNumber);
	Branch branch = bank->branches[branchID];

	// Intitialize and lock branch
	pthread_mutex_init(&(branch.branchLock), NULL);
	pthread_mutex_lock(&(branch.branchLock));

	Account_Adjust(bank, account, -amount, 1);

	// Unlock branch and account
	pthread_mutex_unlock(&(account->accountLock));
	pthread_mutex_unlock(&(branch.branchLock));

	return ERROR_SUCCESS;
}

/*
 * do a tranfer from one account to another account
 */
int
Teller_DoTransfer(Bank *bank, AccountNumber srcAccountNum,
                  AccountNumber dstAccountNum,
                  AccountAmount amount)
{
	assert(amount >= 0);

	DPRINTF('t', ("Teller_DoTransfer(src 0x%"PRIx64", dst 0x%"PRIx64
		          ", amount %"PRId64")\n",
		          srcAccountNum, dstAccountNum, amount));

	Account *srcAccount = Account_LookupByNumber(bank, srcAccountNum);
	if (srcAccount == NULL) {
	 return ERROR_ACCOUNT_NOT_FOUND;
	}

	Account *dstAccount = Account_LookupByNumber(bank, dstAccountNum);
	if (dstAccount == NULL) {
	 return ERROR_ACCOUNT_NOT_FOUND;
	}

	if (amount > Account_Balance(srcAccount)) {
	 return ERROR_INSUFFICIENT_FUNDS;
	}

	/*
	* If we are doing a transfer within the branch, we tell the Account module to
	* not bother updating the branch balance since the net change for the
	* branch is 0.
	*/
	int updateBranch = !Account_IsSameBranch(srcAccountNum, dstAccountNum);

	// Initialize and lock both accounts
	pthread_mutex_init(&(srcAccount->accountLock), NULL);
	pthread_mutex_init(&(dstAccount->accountLock), NULL);
	pthread_mutex_lock(&(srcAccount->accountLock));
	pthread_mutex_lock(&(dstAccount->accountLock));

	Branch srcBranch, dstBranch;

	if (updateBranch) {

		// Find the branches the accounts are contained in
		uint64_t srcBranchID = AccountNum_GetBranchID(srcAccount->accountNumber);
		srcBranch = bank->branches[srcBranchID];
		uint64_t dstBranchID = AccountNum_GetBranchID(dstAccount->accountNumber);
		dstBranch = bank->branches[dstBranchID];

		// Initialize and lock both branches
		pthread_mutex_init(&(srcBranch.branchLock), NULL);
		pthread_mutex_lock(&(srcBranch.branchLock));
		pthread_mutex_init(&(dstBranch.branchLock), NULL);
		pthread_mutex_lock(&(dstBranch.branchLock));

	}

	Account_Adjust(bank, srcAccount, -amount, updateBranch);
	Account_Adjust(bank, dstAccount, amount, updateBranch);

	if (updateBranch) {
		pthread_mutex_unlock(&(srcBranch.branchLock));
		pthread_mutex_unlock(&(srcBranch.branchLock));
	}

	return ERROR_SUCCESS;
}
