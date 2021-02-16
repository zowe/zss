const findCookieInResponse = (response, cookieName) => {
  let cookiesSetByServer = response.headers['set-cookie'];
  let authenticationCookie = cookiesSetByServer.filter(cookieRow => cookieRow.startsWith(cookieName));
  if(authenticationCookie.length === 0) {
    throw new Error('The authentication was unsuccessful');
  }

  return authenticationCookie[0];
};

// export constants and methods
module.exports = {
  findCookieInResponse
};