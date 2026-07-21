create table if not exists public.player_reward_claims (
  user_id uuid not null references auth.users(id) on delete cascade,
  match_id uuid not null,
  reward integer not null,
  coin_reward integer not null,
  score_after integer,
  coin_after integer,
  created_at timestamptz not null default now(),
  primary key (user_id, match_id)
);

alter table public.player_reward_claims enable row level security;

drop policy if exists "Users can read own reward claims" on public.player_reward_claims;
create policy "Users can read own reward claims"
on public.player_reward_claims
for select
to authenticated
using (auth.uid() = user_id);

revoke all on table public.player_reward_claims from anon;
revoke all on table public.player_reward_claims from authenticated;
grant select on table public.player_reward_claims to authenticated;

drop function if exists public.award_win_reward();

create or replace function public.award_win_reward(p_match_id uuid)
returns jsonb
language plpgsql
security definer
set search_path = public, auth
as $$
declare
  v_user_id uuid := auth.uid();
  score_reward int4;
  coin_reward int4;
  new_score int4;
  new_coin int4;
  existing_claim public.player_reward_claims%rowtype;
begin
  if v_user_id is null then
    raise exception 'Not authenticated' using errcode = '28000';
  end if;

  if p_match_id is null then
    raise exception 'Match id is required' using errcode = '22004';
  end if;

  score_reward := floor(random() * 41 + 10)::int4;
  coin_reward := floor(random() * 51 + 50)::int4;

  insert into public.player_reward_claims (
    user_id,
    match_id,
    reward,
    coin_reward
  )
  values (
    v_user_id,
    p_match_id,
    score_reward,
    coin_reward
  )
  on conflict (user_id, match_id) do nothing;

  if not found then
    select *
    into existing_claim
    from public.player_reward_claims
    where user_id = v_user_id
      and match_id = p_match_id;

    return jsonb_build_object(
      'success', true,
      'already_claimed', true,
      'match_id', p_match_id,
      'reward', coalesce(existing_claim.reward, 0),
      'score', existing_claim.score_after,
      'coin_reward', coalesce(existing_claim.coin_reward, 0),
      'coin', existing_claim.coin_after
    );
  end if;

  update public.player_ranks
  set
    score = score + score_reward,
    updated_at = now()
  where user_id = v_user_id
  returning score into new_score;

  if new_score is null then
    raise exception 'Rank not found' using errcode = 'P0002';
  end if;

  update public.player_wallets
  set
    coin = coin + coin_reward,
    updated_at = now()
  where user_id = v_user_id
  returning coin into new_coin;

  if new_coin is null then
    raise exception 'Wallet not found' using errcode = 'P0002';
  end if;

  update public.player_reward_claims
  set
    score_after = new_score,
    coin_after = new_coin
  where user_id = v_user_id
    and match_id = p_match_id;

  return jsonb_build_object(
    'success', true,
    'already_claimed', false,
    'match_id', p_match_id,
    'reward', score_reward,
    'score', new_score,
    'coin_reward', coin_reward,
    'coin', new_coin
  );
end;
$$;

revoke execute on function public.award_win_reward(uuid) from public;
revoke execute on function public.award_win_reward(uuid) from anon;
grant execute on function public.award_win_reward(uuid) to authenticated;
