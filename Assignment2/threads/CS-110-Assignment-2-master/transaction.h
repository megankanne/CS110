#ifndef _TRANSACTION_H
#define _TRANSACTION_H


#include "account.h"

int Transaction_DoAction(Bank *bank, Action *actionPtr, int workerNum, 
			AccountAmount fixedBankBalance);


#endif /* _TRANSACTION_H */
