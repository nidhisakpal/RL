# Parse selected FSTs and check which terminals are covered
selected_fsts = []
with open('results9/solution_iter1.txt', 'r') as f:
    in_section = False
    for line in f:
        line = line.strip()
        if line.startswith('% fs'):
            in_section = True
            # Parse FST: "% fs13: 10 5"
            parts = line.split()
            fst_id = parts[1].rstrip(':')
            # Get terminals in this FST (all parts except the fst_id)
            terminals = []
            for i in range(2, len(parts)):
                if parts[i] == 'S' or parts[i] == 'T':
                    break
                try:
                    terminals.append(int(parts[i]))
                except:
                    break
            if terminals:
                selected_fsts.append((fst_id, terminals))
        elif in_section and line and not line.startswith('%'):
            break

print(f"Selected {len(selected_fsts)} FSTs:")
covered = set()
for fst_id, terminals in selected_fsts:
    print(f"  {fst_id}: terminals {terminals}")
    covered.update(terminals)

print(f"\nCovered terminals: {sorted(covered)}")
print(f"Total covered: {len(covered)}/20")
uncovered = set(range(20)) - covered
if uncovered:
    print(f"Uncovered terminals: {sorted(uncovered)}")
else:
    print("All terminals covered!")
