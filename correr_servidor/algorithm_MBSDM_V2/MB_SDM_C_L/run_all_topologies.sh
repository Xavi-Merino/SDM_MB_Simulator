#!/bin/bash

# Get the current directory name
SESSION_NAME=$(basename "$PWD")

# Start a new tmux session with the session name being the current directory
tmux new-session -d -s "$SESSION_NAME" -n "$SESSION_NAME"

# Split the first window into two additional panes (3 panes total)
tmux split-window -h  # Creates the second pane horizontally
tmux split-window -h  # Creates the third pane horizontally

# Evenly distribute the panes horizontally
tmux select-layout even-horizontal

# Attach to the session
tmux attach-session -t "$SESSION_NAME"