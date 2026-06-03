# Automation App

Desktop automation application with mouse, keyboard, and window control capabilities.

## Features

- 🖱️ **Mouse Control** - Automate mouse movements and clicks
- ⌨️ **Keyboard Control** - Simulate keyboard input
- 🪟 **Window Management** - Control and manipulate application windows
- 🌐 **REST API** - HTTP endpoints for remote control
- 🔌 **WebSocket Support** - Real-time communication

## Tech Stack

- **Node.js** - Runtime environment
- **Express** - Web server framework
- **@nut-tree-fork/nut-js** - Cross-platform automation library
- **node-window-manager** - Window management utilities
- **ws** - WebSocket implementation
- **pkg** - Package into executable

## Installation

```bash
npm install
```

## Usage

### Development
```bash
npm start
```

### Build Executable (Windows)
```bash
npm run build
```

This creates `dist/backend.exe` - a standalone Windows executable.

## API Endpoints

The server exposes REST endpoints for automation control. See the source code for available routes.

## Project Structure

```
automation-app/
├── src/
│   └── server.js      # Main entry point
├── public/            # Static assets
├── dist/              # Built executables
├── package.json       # Project configuration
└── README.md          # This file
```

## Configuration

The `pkg` configuration in `package.json` specifies:
- Target platform: Windows x64
- Node version: 18
- Compression: GZip
- Included assets: `public/**/*`

## Requirements

- Node.js 18+
- npm

## License

MIT
