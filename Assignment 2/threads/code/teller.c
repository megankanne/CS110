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

BranchID GetBranchID(AccountNumber accountNum)
{
  Y;
  return (BranchID) (accountNum >> 32);
}

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

	/// Initialize and lock account
	int val = pthread_mutex_init(&(account->accountLock), NULL);
	//printf("%d", val);
	pthread_mutex_lock(&(account->accountLock));

	// Find the branch the account is contained in
	uint64_t branchID = GetBranchID(account->accountNumber);
	//printf("%u\n", branchID);
	Branch *branch = &(bank->branches[branchID]);

	// Initialize and lock branch
	pthread_mutex_init(&(branch->branchLock), NULL);
	pthread_mutex_lock(&(branch->branchLock));

	Account_Adjust(bank, account, amount, 1);

	// Unlock branch and account
	pthread_mutex_unlock(&(branch->branchLock));
	pthread_mutex_unlock(&(account->accountLock));
	
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
	
	// Initialize account lock
	pthread_mutex_init(&(account->accountLock), NULL);
	// here
	// Lock account 
	pthread_mutex_lock(&(account->accountLock));

	if (amount > Account_Balance(account)) {
		// Unlock account
		pthread_mutex_unlock(&(account->accountLock));
		return ERROR_INSUFFICIENT_FUNDS;
	}

	// Get the account's branch
	uint64_t branchID = GetBranchID(account->accountNumber);
	Branch *branch = &(bank->branches[branchID]);
	// Initialize branch lock
	pthread_mutex_init(&(branch->branchLock), NULL);
	// Lock the branch
	pthread_mutex_lock(&(branch->branchLock));
	
	Account_Adjust(bank, account, -amount, 1);

	// Unlock branch
	pthread_mutex_unlock(&(branch->branchLock));
	// Unlock account
	pthread_mutex_unlock(&(account->accountLock));
	
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

	/*
	* If we are doing a transfer within the branch, we tell the Account module to
	* not bother updating the branch balance since the net change for the
	* branch is 0.
	*/
	int updateBranch = !Account_IsSameBranch(srcAccountNum, dstAccountNum);

	// Initialize and lock both accounts
	pthread_mutex_init(&(srcAccount->accountLock), NULL);
	pthread_mutex_init(&(dstAccount->accountLock), NULL);
	//printf("lock: %x", &(srcAccount->accountLock));
	pthread_mutex_lock(&(srcAccount->accountLock));
	if(srcAccountNum != dstAccountNum){
		pthread_mutex_lock(&(dstAccount->accountLock));
	}else{
		//printf("same account\n");
	}
	
	if (amount > Account_Balance(srcAccount)) {
		pthread_mutex_unlock(&(srcAccount->accountLock));
	 	return ERROR_INSUFFICIENT_FUNDS;
	}

	Branch *srcBranch, *dstBranch;

	if (updateBranch) {

		// Find the branches the accounts are contained in
		uint64_t srcBranchID = GetBranchID(srcAccount->accountNumber);
		uint64_t dstBranchID = GetBranchID(dstAccount->accountNumber);
		
		//correct order so branch with lower ID always locked first
		if(dstBranchID < srcBranchID){
			uint64_t temp;
			temp = srcBranchID;
			srcBranchID = dstBranchID;
			dstBranchID = temp;
		}

		srcBranch = &(bank->branches[srcBranchID]);
		dstBranch = &(bank->branches[dstBranchID]);
		
		// Initialize and lock both branches
		int val = pthread_mutex_init(&(srcBranch->branchLock), NULL);
		pthread_mutex_lock(&(srcBranch->branchLock));
		if(val != 0) {printf("%d", val);}
		pthread_mutex_init(&(dstBranch->branchLock), NULL);
		pthread_mutex_lock(&(dstBranch->branchLock));
	}

	Account_Adjust(bank, srcAccount, -amount, updateBranch);
	Account_Adjust(bank, dstAccount, amount, updateBranch);

	if (updateBranch) {
		pthread_mutex_unlock(&(srcBranch->branchLock));
		pthread_mutex_unlock(&(dstBranch->branchLock));
	}
	
	pthread_mutex_unlock(&(srcAccount->accountLock));
	if(srcAccountNum != dstAccountNum){
		pthread_mutex_unlock(&(dstAccount->accountLock));
	}

	return ERROR_SUCCESS;
}
