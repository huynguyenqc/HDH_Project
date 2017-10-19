// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

#define ABS(x) ((x) < 0 ? -(x) : (x))

int getlen(int s) {
	int len = 0;
	int oneChar;
	do {
		machine->ReadMem(s+len, 1, &oneChar);
		if ((char)oneChar)
			++len;
		else
			break;
	} while (true);
	return len;
}

void
AdjustPCRegs()
{
	int pc;

	pc = machine->ReadRegister(PCReg);
	machine->WriteRegister(PrevPCReg, pc);
	pc = machine->ReadRegister(NextPCReg);
	machine->WriteRegister(PCReg, pc);
	pc += 4;
	machine->WriteRegister(NextPCReg, pc);
}

char*
User2System(int virtAddr, int limit)
{
	int i;
	int oneChar;
	char *kernelBuf = NULL;

	kernelBuf = new char[limit + 1];
	if (kernelBuf == NULL)
		return kernelBuf;

	memset(kernelBuf, 0, limit + 1);

	for (i = 0; i < limit; ++i) {
		machine->ReadMem(virtAddr + i, 1, &oneChar);
		kernelBuf[i] = (char)oneChar;
		if (oneChar == 0)
			break;
	}

	return kernelBuf;
}

int
System2User(int virtAddr, int len, char *buffer)
{
	if (len < 0) return -1;
	if (len == 0) return len;
	int i = 0;
	int oneChar = 0;
	do {
		oneChar = (int) buffer[i];
		machine->WriteMem(virtAddr + i, 1, oneChar);
		++i;
	} while (i < len && oneChar != 0);

	return i;
}

void
SyscallPrintChar()
{
	char character;
	character = (char)machine->ReadRegister(4);
	gSynchConsole->Write(&character, 1);
	machine->WriteRegister(2, 0);
	AdjustPCRegs();
}

void SyscallReadChar() {	// char ReadChar();
	char character;
	gSynchConsole->Read(&character, 1);
	machine->WriteRegister(2, character); // return character
	AdjustPCRegs();
}

void SyscallPrintString() {	// void PrintString(char[] buffer);
	int strAddr = machine->ReadRegister(4);
	int len = getlen(strAddr);

	char* kernelBuff = User2System(strAddr, len);

	gSynchConsole->Write(kernelBuff, len);

	delete[] kernelBuff;

	machine->WriteRegister(2, 0);
	AdjustPCRegs();
}

void SyscallReadString() { 	// void ReadString(char[] buffer, int length);
	int strAddr = machine->ReadRegister(4);
	int len = machine->ReadRegister(5);

	char* kernelBuff = new char[len];
	memset(kernelBuff, 0, len);

	gSynchConsole->Read(kernelBuff, len);
	
	int realLen = System2User(strAddr, len, kernelBuff);	// realLen <= len

	int zero = 0;
	machine->WriteMem(strAddr+realLen, 1, zero);	// character strAddr[realLen] = 0

	delete[] kernelBuff;

	machine->WriteRegister(2, 0);
	AdjustPCRegs();
}

void
SyscallPrintInt()
{
	int number, abs_number, len, i, j;
	char tmp;
	number = machine->ReadRegister(4);
	abs_number = ABS(number);
	len = 0;
	char *str = new char[13];
	do {
		str[len++] = (char)(abs_number % 10 + '0');
		abs_number /= 10;
	} while (abs_number);
	if (abs_number < 0)
		str[len++] = '-';
	for (i = 0; i < len / 2; ++i) {
		j = len - i - 1;
		tmp = str[i];
		str[i] = str[j];
		str[j] = tmp;
	}
	str[len++] = '\0';
}

void
ExceptionHandler(ExceptionType which)
{
	int type = machine->ReadRegister(2);
//    if ((which == SyscallException) && (type == SC_Halt)) {
//	DEBUG('a', "Shutdown, initiated by user program.\n");
//   	interrupt->Halt();
//    } else {
//	printf("Unexpected user mode exception %d %d\n", which, type);
//	ASSERT(FALSE);
//    }
//
	switch (which) {
		case NoException:
			break;
		case PageFaultException: // No valid translation found
			printf("No valid translation found\n");
			interrupt->Halt();
			break;
		case ReadOnlyException: // Write attempted to page marked "read-only"
			printf("Write attempted to page marked \"read-only\"\n");
			interrupt->Halt();
			break;
		case BusErrorException: // Translation resulted in an invalid physical address
			printf("Translation resulted in an invalid physical address\n");
			interrupt->Halt();
			break;
		case AddressErrorException: // Unaligned reference or one that was beyond the end of the address space\n");
			printf("Unaligned reference or one that was beyond the end of the address space\n");
			interrupt->Halt();
			break;
		case OverflowException: // Integer overflow in add or sub.
			printf("Integer overflow in add or sub.\n");
			interrupt->Halt();
			break;
		case IllegalInstrException: // Unimplemented or reserved instr.
			printf("Unimplemented or reserved instr.\n");
			interrupt->Halt();
			break;
		case SyscallException:
			switch (type) {
			case SC_Halt:
				interrupt->Halt();
				break;
			case SC_ReadInt:
				break;
			case SC_PrintInt:
				break;
			case SC_ReadChar:
				SyscallReadChar();
				break;
			case SC_PrintChar:
				SyscallPrintChar();
				break;
			case SC_ReadString:
				SyscallReadString();
				break;
			case SC_PrintString:
				SyscallPrintString();
				break;
			case SC_Exit:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Exec:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Join:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Create:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Open:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Read:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Write:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Close:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Fork:
				// Not implemented yet
				interrupt->Halt();
				break;
			case SC_Yield:
				// Not implemented yet
				interrupt->Halt();
				break;
			default:
				printf("Unexpected user mode exception %d %d\n", which, type);
				ASSERT(FALSE);
				break;
			}
			break;
		default:
			printf("Unexpected user mode exception %d %d\n", which, type);
			ASSERT(FALSE);
			break;
	}
}
