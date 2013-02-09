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

/*
 * Deposit money into an account
 * We lock the account because it's amount is changing
 * We also lock the branch because it's balance is changing
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

	// Initialize account lock
	pthread_mutex_init(&(account->accountLock), NULL);
	// Lock account 
	pthread_mutex_lock(&(account->accountLock));
	// Get the account's branch
	uint64_t branchID = AccountNum_GetBranchID(account->accountNumber);
	Branch branch = bank->branches[branchID];
	// Initialize branch lock
	pthread_mutex_init(&(branch.branchLock), NULL);
	// Lock the branch
	pthread_mutex_lock(&(branch.branchLock));
	
	Account_Adjust(bank, account, amount, 1);

	// Unlock account
	pthread_mutex_unlock(&(account->accountLock));
	// Unlock branch
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

	// Initialize account lock
	pthread_mutex_init(&(account->accountLock), NULL);
	// Lock account 
	pthread_mutex_lock(&(account->accountLock));
	// Get the account's branch
	uint64_t branchID = AccountNum_GetBranchID(account->accountNumber);
	Branch branch = bank->branches[branchID];
	// Initialize branch lock
	pthread_mutex_init(&(branch.branchLock), NULL);
	// Lock the branch
	pthread_mutex_lock(&(branch.branchLock));
	
	Account_Adjust(bank, account, -amount, 1);

	// Unlock account
	pthread_mutex_unlock(&(account->accountLock));
	// Unlock branch
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
		
	// Initialize src account lock
	pthread_mutex_init(&(srcAccount->accountLock), NULL);
	// Initialize dst account lock
	pthread_mutex_init(&(dstAccount->accountLock), NULL);
	
	// Lock src account 
	pthread_mutex_lock(&(srcAccount->accountLock));
	if (amount > Account_Balance(srcAccount)) {
		// Unlock src account
		pthread_mutex_unlock(&(srcAccount->accountLock));
		return ERROR_INSUFFICIENT_FUNDS;
	}
	
	// We don't want to lock the same account twice so check if same and only lock
	// dst if not the same
	if(srcAccountNum != dstAccountNum){
		// Lock dst account 
		pthread_mutex_lock(&(dstAccount->accountLock));
	}
	
	//correct order for bank balance?

	/*
	 * If we are doing a transfer within the branch, we tell the Account module to
	 * not bother updating the branch balance since the net change for the
	 * branch is 0.
	 */
	int updateBranch = !Account_IsSameBranch(srcAccountNum, dstAccountNum);
	
	Branch srcBranch, dstBranch;
	
	// Check that the dst and src branches are not the same
	// Lock both branches iff branches are different
	if(updateBranch){
		// Get the src account's branch
		uint64_t srcBranchID = AccountNum_GetBranchID(srcAccount->accountNumber);
		srcBranch = bank->branches[srcBranchID];
		// Initialize src branch lock
		pthread_mutex_init(&(srcBranch.branchLock), NULL);
		// Lock the src branch
		pthread_mutex_lock(&(srcBranch.branchLock));
		
		// Get the dst account's branch
		uint64_t dstBranchID = AccountNum_GetBranchID(dstAccount->accountNumber);
		dstBranch = bank->branches[dstBranchID];
		// Initialize dst branch lock
		pthread_mutex_init(&(dstBranch.branchLock), NULL);
		// Lock the dst branch
		pthread_mutex_lock(&(dstBranch.branchLock));
	}
	
	Account_Adjust(bank, srcAccount, -amount, updateBranch);
	Account_Adjust(bank, dstAccount, amount, updateBranch);
	
	// Unlock src account
	pthread_mutex_unlock(&(srcAccount->accountLock));
	// Unlock dst account
	pthread_mutex_unlock(&(dstAccount->accountLock));

	if(updateBranch){
		// Unlock src branch
		pthread_mutex_unlock(&(srcBranch.branchLock));
		// Unlock dst branch
		pthread_mutex_unlock(&(dstBranch.branchLock));
	}
	
	return ERROR_SUCCESS;
}
