```markdown
# Dynamic Hash Table Prototype (C back-end + minimal Web UI)

This prototype demonstrates several collision resolution techniques (chaining, linear probing, quadratic probing, double hashing)
and emits JSON snapshots that a simple web UI can visualize.

Files:
- src/*.c, src/*.h : C implementation of the hash table and a driver that generates snapshots.
- ui/index.html, ui/app.js, ui/style.css : Minimal web UI to display snapshots.
- ui/snapshots/ : Directory where the C driver writes snapshot JSON files and manifest.json.
- Makefile : builds bin/ht

Build and run:
1. Build the C program:
   make

2. Produce snapshots (examples):
   ./bin/ht --strategy=CHAINING --size=101 --inserts=500 --dist=uniform --interval=5

   Options:
   --strategy=CHAINING|LINEAR|QUADRATIC|DOUBLE
   --size=N           (hash table size)
   --inserts=N        (how many keys to insert)
   --dist=sequential|uniform|clustered
   --interval=M       (emit snapshot every M inserts)

3. Serve the UI directory (static server). From the project root:
   python3 -m http.server 8000

4. Open in a browser:
   http://localhost:8000/ui/

Notes:
- This is a simple educational prototype. It avoids heavy external deps and focuses on producing clear JSON snapshots.
- The UI polls a manifest.json to discover snapshots and animates them.
```