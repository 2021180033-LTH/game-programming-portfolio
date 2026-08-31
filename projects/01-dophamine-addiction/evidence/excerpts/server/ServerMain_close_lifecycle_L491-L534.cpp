/*
 * PORTFOLIO CODE EXCERPT — NOT STANDALONE OR BUILDABLE BY ITSELF
 * Fixed source commit: 4cb26cc0cbcf8e530ecd662277ab697c236a45ad
 * Original path: Server/ServerMain.cpp
 * Original line range(s): 491-534
 * Attribution evidence: current git blame at the fixed commit: 44/44 lines attributed to Lee Taehyun identities
 * Review purpose: single-close guard and client closing lifecycle
 * The surrounding team code is intentionally omitted.
 */

// ---- Verbatim source lines 491-534 ----
void CloseClientSocketOnce(ClientContext* c) {
    if (!c) {
        return;
    }

    AcquireSRWLockExclusive(&c->closeLock);

    const SOCKET sock = c->sock;
    if (sock == INVALID_SOCKET) {
        ReleaseSRWLockExclusive(&c->closeLock);
        return;
    }

    c->sock = INVALID_SOCKET;
    ReleaseSRWLockExclusive(&c->closeLock);

    shutdown(sock, SD_BOTH);
    closesocket(sock);
}

void MarkClientClosing(ClientContext* c, const char* reason) {
    if (!c) {
        return;
    }

    const bool firstClose = !c->closing.exchange(true, std::memory_order_acq_rel);
    if (firstClose) {
        IOCP_TRACE("[CONN] CLOSE reason=%s to=%d.%d.%d.%d\n",
            reason ? reason : "Unknown",
            c->addr.sin_addr.S_un.S_un_b.s_b1,
            c->addr.sin_addr.S_un.S_un_b.s_b2,
            c->addr.sin_addr.S_un.S_un_b.s_b3,
            c->addr.sin_addr.S_un.S_un_b.s_b4);
        g_lobby.OnClientDisconnected(c);
    }

    CloseClientSocketOnce(c);

    if (firstClose &&
        !c->baseRefReleased.exchange(true, std::memory_order_acq_rel)) {
        ReleaseIO(c);
    }
}


