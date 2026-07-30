<template>
  <div id="app">
    <header>
      <h1>Alipay Sandbox Payment Test</h1>
    </header>
    <main>
      <router-view />
    </main>
    <footer>
      <p>Test interface for Alipay sandbox payment flows</p>
    </footer>
  </div>
</template>

<script setup>
// Root component - no additional logic needed
</script>

<style>
* {
  margin: 0;
  padding: 0;
  box-sizing: border-box;
}

body {
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, sans-serif;
  background-color: #ffffff;
  color: #000000;
}

#app {
  min-height: 100vh;
  display: flex;
  flex-direction: column;
}

header {
  background-color: #f5f5f5;
  padding: 20px;
  text-align: center;
  border-bottom: 1px solid #e0e0e0;
}

header h1 {
  font-size: 24px;
  font-weight: bold;
}

main {
  flex: 1;
  max-width: 800px;
  width: 100%;
  margin: 0 auto;
  padding: 40px 20px;
}

footer {
  background-color: #f5f5f5;
  padding: 20px;
  text-align: center;
  border-top: 1px solid #e0e0e0;
  font-size: 14px;
  color: #666666;
}
</style>
