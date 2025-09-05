import sys

transaction_table = {}
lock_table = {}
TC = 1

def begin_transaction(tid, out):
    global TC
    transaction_table[tid] = {
        "status" : "active",
        "timestamp": TC,
        "items_locked" : set(),
        "waiting_op" : []
    }
    out.write(f"b{tid}; T{tid} begins ID={tid} TS={transaction_table[tid]['timestamp']} State={transaction_table[tid]['status']}.\n")
    TC+=1
    
def read_transaction(tid, item, out):
    status = transaction_table[tid]["status"]
    if status == "active" or status == "waiting":
        # Adds item to the lock table if it does not already exist
        if item not in lock_table:
            lock_table[item] = {
                "lock_type": "read",
                "holders": {tid},
                "waiting": set()
            }
            # adds item to the transaction table for that transaction
            transaction_table[tid]["items_locked"].add(item)
            out.write(f"r{tid}({item}); {item} is read locked by T{tid}\n")
            return
        
        lock_info = lock_table[item]
        # adds transaction to lock holder list 
        if lock_info["lock_type"] == "read":
            lock_info["holders"].add(tid)
            transaction_table[tid]["items_locked"].add(item)
            out.write(f"r{tid}({item}); {item} is read locked by T{tid}\n")
            return
        elif lock_info["lock_type"] == "write":
            holding_trxn = lock_info["holders"]
            if tid in holding_trxn:
                out.write(f"r{tid}({item}); {item} is already write locked by T{tid}\n")
                return
                
            wait_die(tid, holding_trxn, item, out, 'r')
    else:
        out.write(f"r{tid}({item}); Transaction T{tid} is already committed/aborted.\n")
    
def write_transaction(tid, item, out):
    lock=lock_table.get(item)
    status = transaction_table[tid]["status"]
    if status == "active" or status == "waiting":
        # Adds item to the lock table if it does not already exist
        if lock is None:
            lock_table[item] = {
                "lock_type": "write",
                "holders": {tid},
                "waiting": set()
                }
            transaction_table[tid]["items_locked"].add(item)
            out.write(f"w{tid}({item}); {item} is write locked by T{tid}\n")
            return
            
        if lock["lock_type"] == "read":
            if lock["holders"] == {tid}:
                lock["lock_type"] = "write"
                out.write(f"w{tid}({item}); read lock on {item} by T{tid} is upgraded to write lock.\n")
            else:
                if wait_die(tid, lock["holders"], item, out,'w'):
                    lock["waiting"].add(tid)
            return
        
        elif lock["lock_type"] == "write":
            if tid in lock["holders"]:
                out.write(f"w{tid}({item}); {item} is already write locked by T{htid}\n")
                return
            else:
                if wait_die(tid, lock["holders"], item, out,'w'):
                    lock["waiting"].add(tid)
            return
               
    else:
        out.write(f"w{tid}({item}); Transaction T{tid} is already committed/aborted.\n")
    return    
        
def end_transaction(tid, out):
    if tid not in transaction_table:
        out.write(f"e{tid}; T{tid} not found.\n")
        return
    status = transaction_table[tid]["status"]
    if status == "aborted" or status == "committed":
        out.write(f"e{tid}; Transaction T{tid} cannot be commited (not active or already commited/aborted)\n")
    else:
        transaction_table[tid]["status"] = "commited"
        out.write(f"e{tid}; Transaction T{tid} is committed.\n")
    release_locks(tid)
    return
    
# This function is used to handle the wait-die protocol
# if output message errors, comment out this function
# and uncomment the alternative implmenetation in write_transaction and read_transaction
def wait_die(tid, holders, item, out, op):
    holders = {h for h in holders if h != tid}
    for holder in holders:
        if transaction_table[tid]['timestamp'] < transaction_table[holder]['timestamp']:
            if transaction_table[tid]["status"] == "active" or transaction_table[tid]["status"] == "waiting":
                out.write(f"T{tid}({item}); T{tid} is waiting on T{holders}\n")
                transaction_table[tid]['status'] = "waiting"
                lock_table[item]["waiting"].add(tid)
                return True
        elif transaction_table[tid] != transaction_table[holder]:
            if transaction_table[tid]["status"] == "active" or transaction_table[tid]["status"] == "waiting": 
                out.write(f"{op}{tid}({item}); Abort T{tid} as it is younger than T{holder} following (wait-die); Transaction T{tid} is aborted.\n")
                transaction_table[tid]["status"] = "aborted"
                release_locks(tid)
            else:
                out.write(f"{op}{tid}({item}); Transaction already committed/aborted\n")
            return False  # Abort
    return True
        
def read_file(filename):
    with open(filename, 'r') as f:
        lines = f.readlines()
    
    operations = []
    for line in lines:
        line = line.strip().rstrip(';')
        if line:
            operations.append(line)
    return operations
   
def process_operations(operations, out):
    for op in operations:
        if op.startswith('b'):
            tid = int(op[1])
            begin_transaction(tid, out)
        elif op.startswith('r'):
            tid = int(op[1])
            item = op[3]
            read_transaction(tid, item, out)
        elif op.startswith('w'):
            tid = int(op[1])
            item = op[3]
            write_transaction(tid, item, out)
        elif op.startswith('e'):
            tid = int(op[1])
            end_transaction(tid, out)
 
def release_locks(tid):
    # releases locks for transactions that have ended
    for item in transaction_table[tid]["items_locked"]:
        if item in lock_table:    
            lock_info = lock_table[item]
            lock_info["holders"].discard(tid)
            if not lock_info["holders"]:        ## if no holders left, remove lock
                del lock_table[item]
    transaction_table[tid]["items_locked"].clear()

def main():
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    operations = read_file(input_file)
    
    with open(output_file, 'w') as out:
        process_operations(operations, out)
        
if __name__ == "__main__":
    main()