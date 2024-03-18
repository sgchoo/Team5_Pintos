#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/filesys.h"
#include "include/filesys/file.h"

void		syscall_entry (void);
void		check_syscall_handler(struct intr_frame *);
void		syscall_handler (struct intr_frame *);

bool		check_valid_address(uint64_t *);

void		halt(void);
void		exit(int);
tid_t		fork(const char *);
int			exec(const char *);
int			wait(tid_t);
int 		write (int, const void *, unsigned);
bool 		create(const char *, unsigned);
bool 		remove(const char *);
int 		open(const char *);
int 		filesize(int);
int			read(int, void *, unsigned);
int 		write(int, const void *, unsigned);
void 		seek(int , unsigned);
unsigned 	tell(int);
void 		close(int);

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f) {
	// TODO: Your implementation goes here.
	check_syscall_handler(f);
	// thread_exit ();
}

void
check_syscall_handler(struct intr_frame *if_)
{
	switch(if_->R.rax)
	{
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(if_->R.rdi);
			break;
		case SYS_FORK:
			// if_->R.rax = fork(if_->R.rdi);
			break;
		case SYS_EXEC:
			if((exec(if_->R.rdi)) == -1)
				exit(-1);
			break;
		case SYS_WAIT:
			// if_->R.rax = wait(if_->R.rdi);
			break;
		case SYS_CREATE:
			if(check_valid_address(if_->R.rdi))
				if_->R.rax = create(if_->R.rdi, if_->R.rsi);
			else
				exit(-1);	
			break;
		case SYS_REMOVE:
			if(check_valid_address(if_->R.rsi))
				if_->R.rax = remove(if_->R.rdi);
			else
				exit(-1);
			break;
		case SYS_OPEN:
			if(check_valid_address(if_->R.rdi))
				if_->R.rax = open(if_->R.rdi);
			else
				exit(-1);
			break;
		case SYS_FILESIZE:
			if_->R.rax = filesize(if_->R.rdi);
			break;
		case SYS_READ:
			if(check_valid_address(if_->R.rsi))
				if_->R.rax = read(if_->R.rdi, if_->R.rsi, if_->R.rdx);
			else
				exit(-1);
			break;
		case SYS_WRITE:
			if(check_valid_address(if_->R.rsi))
				if_->R.rax = write(if_->R.rdi, if_->R.rsi, if_->R.rdx);
			else
				exit(-1);
			break;
		case SYS_SEEK:
			seek(if_->R.rdi, if_->R.rsi);
			break;
		case SYS_TELL:
			if_->R.rax = tell(if_->R.rdi);
			break;
		case SYS_CLOSE:
			close(if_->R.rdi);
			break;
	}
}

bool
check_valid_address(uint64_t *address)
{
	struct	thread *cur_thread = thread_current();

	bool is_valid = true;

	if(	address == NULL \
		|| is_kernel_vaddr(address) \
		|| pml4_get_page(cur_thread->pml4, address) == NULL)
		return is_valid = false;

	return is_valid;
}

void
halt(void)
{
	power_off();
}

void
exit(int status)
{
	struct	thread *cur_thread = thread_current();

	cur_thread->exit_status = status;
	printf("%s: exit(%d)\n", cur_thread->name, cur_thread->exit_status);

	thread_exit();
}

// tid_t
// fork(const char *thread_name)
// {
// 	return process_fork(thread_name, &thread_current()->tf);
// }

int
exec(const char *cmd_line)
{
	int success = 0;

	// if(check_valid_address(thread_current()->tf.rip))
	// 	return thread_current()->exit_status - 1;

	// success = process_exec(cmd_line);

	// if(success == -1)
	// 	return thread_current()->exit_status - 1;
}

// int
// wait(tid_t pid)
// {
// 	return process_wait(pid);
// }

bool
create(const char *file, unsigned initial_size)
{
	if(filesys_create(file, initial_size))
		return true;
	else
		return false;
}

bool
remove(const char *file)
{
	if(filesys_remove(file))
		return true;
	else
		return false;
}

int
open(const char *file)
{
	struct	thread	*cur_thread = thread_current();
	struct	file	*temp_file = filesys_open(file);

	if(temp_file)
	{
		int old_fd = cur_thread->fd;
		cur_thread->file_table[cur_thread->fd] = temp_file;
		cur_thread->fd++;
		return old_fd;
	}
	else
		return -1;
}

int
filesize(int fd)
{
	struct file	*target_file = thread_current()->file_table[fd];

	if(fd < 0 || fd >= 64)
	{
		return -1;
	}

	if(target_file)
		return file_length(target_file);
}

int
read(int fd, void *buffer, unsigned size)
{
	struct file *target_file;

	if(fd < 0 || fd > 63)
		return -1;
	else if(fd == 1 || fd == 2)
		return -1;
	else if(fd == 0)
		input_getc();
	
	target_file = thread_current()->file_table[fd];

	if(target_file)
		return file_read(target_file, buffer, size);
}

int
write(int fd, const void *buffer, unsigned size)
{	
	struct file *target_file;

	if(fd < 0 || fd > 63)
		return -1;
	else if(fd == 0 || fd == 2)
		return -1;
	else if(fd == 1)
		putbuf(buffer, size);

	target_file = thread_current()->file_table[fd];

	if(target_file)
		return file_write(target_file, buffer, size);	
}

void
seek(int fd, unsigned position)
{
	struct file *target_file;

	if(fd < 0 || fd > 63)
	{
		exit(-1);
		return;
	}
	target_file = thread_current()->file_table[fd];
	if(target_file)
		file_seek(target_file, position);
}

unsigned
tell(int fd)
{
	struct file *target_file;

	if(fd < 0 || fd > 63)
	{
		return -1;
	}
	target_file = thread_current()->file_table[fd];
	if(target_file)
		return file_tell(target_file);
}

void
close(int fd)
{
	struct	file *target_file;

	if(fd < 0 || fd >= 64)
	{
		exit(-1);
	}

	target_file = thread_current()->file_table[fd];
	
	if(target_file)
	{
		file_close(target_file);
		thread_current()->file_table[fd] = NULL;
	}
}