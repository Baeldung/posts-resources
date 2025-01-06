#!/usr/bin/env bash
#
# test_simpledb.sh
#
# A comprehensive test script for the 'simpledb' command-line utility.
# This script demonstrates:
#   - Basic command checks
#   - Proper handling of database directories and JSON tables
#   - Interaction with other UNIX utilities (grep, jq, join)
#   - Various use cases and edge cases
#
# Usage:
#   ./test_simpledb.sh
# Make sure 'simpledb' is compiled and available in $PATH or specify its location.

################################################################################
# Helpers and Setup
################################################################################

# If your simpledb binary isn't in the PATH, set SIMPLEDB to the relative path.
# For example: SIMPLEDB=./simpledb
SIMPLEDB="./simpledb"

# We create two test DB directories:
DB1="testdb1"
DB2="testdb2"
DB3="testdb3"

# Clean up any old directories from previous tests
rm -rf "$DB1" "$DB2" "$DB3"

# Create fresh directories
mkdir -p "$DB1"
mkdir -p "$DB2"
mkdir -p "$DB3"

echo "=== Starting test of simpledb ==="
echo "=== Using three database directories: $DB1, $DB2 and $DB3 ==="

################################################################################
# 1) Basic Usage / Error Handling
################################################################################

echo ""
echo "### 1) Testing missing arguments or invalid usage..."

echo "- Attempting to run without arguments (expect usage error):"
$SIMPLEDB 2>&1 || true

echo "- Attempting to run with no --db-path (expect usage error):"
$SIMPLEDB list users 2>&1 || true

echo "- Attempting to run with --db-path but no command (expect usage error):"
$SIMPLEDB --db-path "$DB1" 2>&1 || true

echo "- Attempting an unknown command (expect error):"
$SIMPLEDB --db-path "$DB1" unknowncmd mytable 2>&1 || true

################################################################################
# 2) Basic Create and List
################################################################################

echo ""
echo "### 2) Creating and listing a simple table..."

# Step 2a: 'list' a table that doesn't yet exist (should be empty/created).
echo "- Listing 'users' in $DB1 (should be empty JSON array):"
$SIMPLEDB --db-path "$DB1" list users

echo "- Checking the content of $DB1/users.json (should exist after first command, or created empty)."
ls -l "$DB1"

################################################################################
# 3) Save / Update and then List
################################################################################

echo ""
echo "### 3) Save new records and update existing ones..."

echo "- Adding a new record with id=100 to 'users' in $DB1:"
$SIMPLEDB --db-path "$DB1" save users id=100 name="John Doe" age=30 email=john@example.com

echo "- Listing 'users' again (should show John):"
$SIMPLEDB --db-path "$DB1" list users

echo "- Updating record with id=100 (changing email):"
$SIMPLEDB --db-path "$DB1" save users id=100 email=johndoe@newmail.com

echo "- Listing 'users' again (should show updated email):"
$SIMPLEDB --db-path "$DB1" list users

echo "- Adding second record with id=200..."
$SIMPLEDB --db-path "$DB1" save users id=200 name="Alice" age=28 email=alice@example.com

echo "- Listing 'users' to confirm the second record..."
$SIMPLEDB --db-path "$DB1" list users

################################################################################
# 4) Get By Field
################################################################################

echo ""
echo "### 4) Get by field value..."

echo "- Getting users with id=100 (should return John):"
$SIMPLEDB --db-path "$DB1" get users id=100

echo "- Getting users with email=alice@example.com (should return Alice):"
$SIMPLEDB --db-path "$DB1" get users email=alice@example.com

echo "- Getting users with an unknown field (should return no records):"
$SIMPLEDB --db-path "$DB1" get users id=9999

################################################################################
# 5) Delete
################################################################################

echo ""
echo "### 5) Delete records..."

echo "- Deleting user with id=200:"
$SIMPLEDB --db-path "$DB1" delete users id=200

echo "- Listing users (should only have John now):"
$SIMPLEDB --db-path "$DB1" list users

echo "- Trying to delete a non-existent user (id=9999) (should delete 0):"
$SIMPLEDB --db-path "$DB1" delete users id=9999

echo "- Listing users again to confirm no change:"
$SIMPLEDB --db-path "$DB1" list users

################################################################################
# 6) Multiple Tables in the Same DB
################################################################################

echo ""
echo "### 6) Working with multiple tables within the same database..."

echo "- Creating a 'products' table in $DB1..."
$SIMPLEDB --db-path "$DB1" save products id=5001 name="Widget" price=19.99
$SIMPLEDB --db-path "$DB1" save products id=5002 name="Gadget" price=29.99

echo "- Listing 'products':"
$SIMPLEDB --db-path "$DB1" list products

echo "- We still have 'users' table too. Listing 'users' again to check everything is intact:"
$SIMPLEDB --db-path "$DB1" list users

################################################################################
# 7) Second Database Directory (Simultaneously)
################################################################################

echo ""
echo "### 7) Creating and using a second separate database directory ($DB2)..."

echo "- Let's add a 'users' table in $DB2 with different data..."
$SIMPLEDB --db-path "$DB2" save users id=999 name="Jane Doe" email=jane@example.com
$SIMPLEDB --db-path "$DB2" list users

echo "- $DB1 and $DB2 are totally independent. Checking each directory's content:"
echo "Contents of $DB1:"
ls -l "$DB1"
echo "Contents of $DB2:"
ls -l "$DB2"

################################################################################
# 8) Using grep and jq for advanced filtering
################################################################################

echo ""
echo "### 8) Combining simpledb with grep and jq..."

echo "- Let's add a few more users to $DB1's 'users' table..."
$SIMPLEDB --db-path "$DB1" save users id=101 name="Alpha Tester" email=alpha@example.com
$SIMPLEDB --db-path "$DB1" save users id=102 name="Beta Tester" email=beta@example.com
echo "- Now listing all users in JSON lines format:"
$SIMPLEDB --db-path "$DB1" list users

echo ""
echo "#### 8a) Using grep to find user names containing 'Tester':"
$SIMPLEDB --db-path "$DB1" list users | grep '"name":"[^"]*Tester'

echo ""
echo "#### 8b) Using jq to filter by email matching 'example.com':"
$SIMPLEDB --db-path "$DB1" list users | jq 'select(.email | test("example\\.com$"))'

echo ""
echo "#### 8c) Sorting by name with jq:"
$SIMPLEDB --db-path "$DB1" list users | jq -s 'sort_by(.name)[]'

################################################################################
# 9) Simulating a JOIN using standard UNIX tools
################################################################################

echo ""
echo "### 9) Simulating a 'join' between tables..."

# We'll treat 'users' in $DB1 as a table with fields: id, name, email
# We'll create an 'orders' table with fields: order_id, user_id, product, price

echo "- Creating an 'orders' table in $DB1..."
$SIMPLEDB --db-path "$DB1" save orders order_id=9001 user_id=100 product="Red Book" price=15.00
$SIMPLEDB --db-path "$DB1" save orders order_id=9002 user_id=101 product="Blue Pen" price=2.50
$SIMPLEDB --db-path "$DB1" save orders order_id=9003 user_id=999 product="Green Pencil" price=1.00
echo "- Listing 'orders':"
$SIMPLEDB --db-path "$DB1" list orders

echo ""
echo "#### 9a) Converting 'users' to CSV (id,name,email) => users.csv"
$SIMPLEDB --db-path "$DB1" list users | \
  jq -r '[.id, .name, .email] | @csv' > users.csv
cat users.csv

echo ""
echo "#### 9b) Converting 'orders' to CSV (order_id,user_id,product,price) => orders.csv"
$SIMPLEDB --db-path "$DB1" list orders | \
  jq -r '[.order_id, .user_id, .product, .price] | @csv' > orders.csv
cat orders.csv

echo ""
echo "#### 9c) Sort both CSV files by their key for join."
# For users.csv, the key is the first column (id).
# For orders.csv, the key is the second column (user_id), so we want to rearrange or join properly.
sort -t, -k1,1 users.csv > users_sorted.csv
sort -t, -k2,2 orders.csv > orders_sorted.csv

echo "- Sorted 'users':"
cat users_sorted.csv
echo "- Sorted 'orders':"
cat orders_sorted.csv

echo ""
echo "#### 9d) Join by user_id (users.id == orders.user_id)"
echo "(We have to specify that for 'users' the join field is column 1, for 'orders' it's column 2)"
join -t, -1 1 -2 2 users_sorted.csv orders_sorted.csv > joined.csv
echo "- Result of join (joined.csv):"
cat joined.csv

echo ""
echo "#### 9e) Explanation:"
echo "The joined.csv lines combine the user info with the order info if the IDs match."

################################################################################
# 10) Testing automatic ID generation and invalid ID values (in a new DB)
################################################################################

echo ""
echo "### 10) Testing automatic and invalid IDs in $DB3..."

echo "- Case A: Save without providing an ID at all (should auto-generate id=1)."
$SIMPLEDB --db-path "$DB3" save people name="Bob" email="bob@example.com"

echo "- Listing 'people' (should see Bob with id=1):"
$SIMPLEDB --db-path "$DB3" list people

echo ""
echo "- Case B: Save another record without ID (auto-generate id=2)."
$SIMPLEDB --db-path "$DB3" save people name="Alice" email="alice@example.com"

echo "- Listing 'people' (should see Bob (id=1) and Alice (id=2)):"
$SIMPLEDB --db-path "$DB3" list people

echo ""
echo "- Case C: Provide a valid positive integer ID."
$SIMPLEDB --db-path "$DB3" save people id=10 name="Charlie" email="charlie@example.com"

echo "- Listing 'people' (should see Bob (id=1), Alice (id=2), and Charlie (id=10)):"
$SIMPLEDB --db-path "$DB3" list people

echo ""
echo "- Case D: Provide an invalid (negative) ID, expecting an error."
$SIMPLEDB --db-path "$DB3" save people id=-5 name="NegativeID" 2>&1 || true

echo "- Listing 'people' again (no changes expected):"
$SIMPLEDB --db-path "$DB3" list people

echo ""
echo "- Case E: Provide an invalid (non-numeric) ID, expecting an error."
$SIMPLEDB --db-path "$DB3" save people id=abc name="NonNumericID" 2>&1 || true

echo "- Listing 'people' again (no changes expected):"
$SIMPLEDB --db-path "$DB3" list people

echo ""
echo "### 10) End of tests for $DB3."

################################################################################
# 11) Final Checks
################################################################################

echo ""
echo "### 11) Final checks and cleanup hints..."

echo "- Database directories currently exist at $DB1, $DB2 and $DB3"
echo "- If you want to remove them, run: rm -rf $DB1 $DB2 $DB3"
echo "- CSV and JSON files (users.csv, orders.csv, etc.) are also in the current directory."


echo ""
echo "=== End of test script for simpledb ==="
exit 0

