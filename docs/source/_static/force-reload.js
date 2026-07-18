window.addEventListener('pageshow', (event) => {
  // If the page was loaded from the browser's back/forward cache
  if (event.persisted) {
    window.location.reload();
  }
});
