const corsHeaders = {
  "Access-Control-Allow-Origin": "*",
  "Access-Control-Allow-Headers": "authorization, x-client-info, apikey, content-type",
  "Access-Control-Allow-Methods": "GET, OPTIONS",
};

function jsonResponse(body: unknown, status: number): Response {
  return new Response(JSON.stringify(body), {
    status,
    headers: { ...corsHeaders, "Content-Type": "application/json" },
  });
}

Deno.serve(async (req) => {
  if (req.method === "OPTIONS") {
    return new Response("ok", { headers: corsHeaders });
  }

  if (req.method !== "GET") {
    return jsonResponse({ error: "Method not allowed." }, 405);
  }

  const authorization = req.headers.get("Authorization") ?? "";
  const token = authorization.replace(/^Bearer\s+/i, "").trim();

  if (!token) {
    return jsonResponse({ error: "Missing bearer token." }, 401);
  }

  const supabaseUrl = Deno.env.get("SUPABASE_URL");
  const anonKey = Deno.env.get("SUPABASE_ANON_KEY");

  if (!supabaseUrl || !anonKey) {
    return jsonResponse({ error: "Server is not configured." }, 500);
  }

  const userResponse = await fetch(`${supabaseUrl}/auth/v1/user`, {
    method: "GET",
    headers: {
      "apikey": anonKey,
      "Authorization": `Bearer ${token}`,
      "Content-Type": "application/json",
    },
  });

  if (!userResponse.ok) {
    return jsonResponse({ error: "Invalid token." }, 401);
  }

  const user = await userResponse.json();
  const nickname =
    user?.user_metadata?.nickname ??
    user?.user_metadata?.username ??
    user?.email ??
    user?.id;

  return jsonResponse(
    {
      id: user.id,
      email: user.email,
      nickname,
    },
    200,
  );
});
