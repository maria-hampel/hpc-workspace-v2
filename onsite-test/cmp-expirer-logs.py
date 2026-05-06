import argparse
import pandas as pd
import pathlib 
import re


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument("-o", type=pathlib.Path, default="/Users/memento/Documents/Code/hpc-workspace-v2/onsite-test/workspace_logs/log_from_old_expirer-2026-04-27-07-52.log", help="path to old file")
    parser.add_argument("-n", type=pathlib.Path, default="/Users/memento/Documents/Code/hpc-workspace-v2/onsite-test/workspace_logs/log_from_new_expirer-2026-04-27-09-50.log", help="path to new file")
    args = parser.parse_args()
    
    old_log_file = args.o
    new_log_file = args.n 
    
    print(f"Path to old log file {old_log_file}, Path to new log file {new_log_file}")
    
    with open(old_log_file) as f:
        old_log = f.read()
        
    with open(new_log_file) as f:
        new_log = f.read()
        
    # Step 1, see if valid workspaces are equal 
    old_step_1 = re.compile(r"valid workspace .*/(\S+)")
    old_step_1_expired = re.compile(r"valid removed workspace .*\/(\S+)")
    new_step_1 = re.compile(r"found valid workspace:.* (\S+)")
    new_step_1_expired = re.compile(r"=>.* ([0-9]*) valid expired")
    
    old_valid_workspaces = old_step_1.findall(old_log)
    new_valid_workspaces = new_step_1.findall(new_log)
    old_valid_expired = old_step_1_expired.findall(old_log)
    new_valid_expired = new_step_1_expired.findall(new_log)

    if set(old_valid_workspaces) == set(new_valid_workspaces):
        print("Step 1: SUCCESS Set of valid workspaces are the same!")
    else: 
        print("Step 1: ERROR Valid workspaces are not the same :c")
        print(f"Difference: {set(old_valid_workspaces).difference(set(new_valid_workspaces))}")
        
    if len(old_valid_expired) == int(new_valid_expired[0]):
        print("Step 1: SUCCESS number of valid expired are the same!")
    else:
        print("Step 1: ERROR number of expired workspaces are not the same")
        print(f"Difference: old_expied: {len(old_valid_expired)}, new: {new_valid_expired[0]}")
        
        
    # find all stray workspaces 
    old_strays = re.compile(r"stray workspace .*\/(\S+)")
    old_deleted_strays = re.compile(r"stray removed workspace .*\/(\S+)")
    new_strays = re.compile(r"stray workspace (\S+)")
    new_deleted_strays = re.compile(r"stray removed workspace (\S+)")

    
    old_stray_workspaces = old_strays.findall(old_log)
    old_deleted_stray_workspaces = old_deleted_strays.findall(old_log)
    new_stray_workspaces = new_strays.findall(new_log)
    new_deleted_stray_workspaces = new_deleted_strays.findall(new_log)
    
    if set(old_stray_workspaces) == set(new_stray_workspaces):
        print("Step 1: SUCCESS Set of stray workspaces are the same!")
    else: 
        print("Step 1: ERROR Stray workspaces are not the same :c")
        print(f"Difference: {set(old_stray_workspaces).difference(set(new_stray_workspaces))}")
        
    if set(old_deleted_stray_workspaces) == set(new_deleted_stray_workspaces):
        print("Step 1: SUCCESS Set of deleted stray workspaces are the same!")
    else:
        print("Step 1: ERROR Deleted stray workspaces are not the same :c")
        print(f"Difference: {set(old_deleted_stray_workspaces).difference(set(new_deleted_stray_workspaces))}")
        
    
    # Step 2, checking for workspaces to be expired 
    old_keeping = re.compile(r"keeping \/.*\/(\S+)")
    old_reminder = re.compile(r"""keeping .*\/(\S+) .*\n.*MAIL .*""")
    new_keeping = re.compile(r"keeping.* left\): (\S+)")
    new_reminder = re.compile(r"keeping.* (\S+).*\n.*mail.*")
    
    old_keeping_workspaces = old_keeping.findall(old_log)
    old_reminder_workspaces = old_reminder.findall(old_log)
    new_keeping_workspaces = new_keeping.findall(new_log)
    new_reminder_workspaces = new_reminder.findall(new_log)
    
    if set(old_keeping_workspaces) == set(new_keeping_workspaces):
        print("Step 2: SUCCESS Set of kept workspaces are the same!")
    else: 
        print("Step 2: ERROR Kept Workspaces are not the same :c")
        print(f"Difference: {set(old_keeping_workspaces).difference(set(new_keeping_workspaces))}")
        
    if set(old_reminder_workspaces) == set(new_reminder_workspaces):
        print("Step 2: SUCCESS Set of reminder-mails are the same!")
    else: 
        print("Step 2: ERROR Set of reminder-mails are not the same :c")
        print(f"Difference: {set(old_reminder_workspaces).difference(set(new_reminder_workspaces))}")
       
    
    # workspaces that should be expired
    old_wouldexpire = re.compile(r"expir.+\/(\S+) \(")
    new_wouldexpire = re.compile(r"expir.+ (\S+) \(")
    
    old_wouldexpire_workspaces = old_wouldexpire.findall(old_log)
    new_wouldexpire_workspaces = new_wouldexpire.findall(new_log)
    
    if set(old_wouldexpire_workspaces) == set(new_wouldexpire_workspaces):
        print("Step 2: SUCCESS Set of (would-be) expired workspaces are the same!")
    else:
        print("Step 2: ERROR Set of (would-be) expired workspaces are not the same :c")
        print(f"Difference: {set(old_wouldexpire_workspaces).difference(set(new_wouldexpire_workspaces))}")
       
       
    # Step 3: Deletion of expired Workspaces 
    old_keepdeleted = re.compile(r"keeping further.*\/(\S+)")
    new_keepdeleted = re.compile(r"keeping.* left\),.* (\S+)")
    
    old_keepdeleted_workspaces = old_keepdeleted.findall(old_log)
    new_keepdeleted_workspaces = new_keepdeleted.findall(new_log)

    if set(old_keepdeleted_workspaces) == set(new_keepdeleted_workspaces):
        print("Step 3: SUCCESS Set of kept workspaces are the same!")
    else: 
        print("Step 3: ERROR Kept Workspaces are not the same :c")
        print(f"Difference: {set(old_keepdeleted_workspaces).difference(set(new_keepdeleted_workspaces))}")

    # workspaces that should be deleted
    old_delete_expired = re.compile(r"delet.+\/(\S+).+expired")
    old_delete_released = re.compile(r"delet.+\/(\S+).+released")
    new_delete_expired = re.compile(r"delete DB.+ (\S+),.*expired")
    new_delete_released = re.compile(r"delete DB.+ (\S+),.*released")
    
    old_delete_expired_workspaces = old_delete_expired.findall(old_log)
    old_delete_released_workspaces = old_delete_released.findall(old_log)
    new_delete_expired_workspaces = new_delete_expired.findall(new_log)
    new_delete_released_workspaces = new_delete_released.findall(new_log)
    
    
    if set(old_delete_expired_workspaces) == set(new_delete_expired_workspaces):
        print("Step 3: SUCCESS Set of (would-be) deleted workspaces (expired) are the same!")
    else: 
        print("Step 3: ERROR Set of (would-be) deleted workspaces (expired) are not the same :c")
        print(f"Difference: {set(old_delete_expired_workspaces).difference(set(new_delete_expired_workspaces))}")
        
    if set(old_delete_released_workspaces) == set(new_delete_released_workspaces):
        print("Step 3: SUCCESS Set of (would-be) deleted workspaces (released) are the same!")
    else: 
        print("Step 3: ERROR Set of (would-be) deleted workspaces (released) are not the same :c")
        print(f"Difference: {set(old_delete_released_workspaces).difference(set(new_delete_released_workspaces))}")


