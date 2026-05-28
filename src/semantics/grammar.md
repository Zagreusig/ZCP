$$
\begin{align}
   [\text{Prog}] &\to [\text{Statement}]^*\\
   [\text{Statement}] &\to 
   \begin{cases}
      \text{exit([\text{Expr}])}; \\
      \text{have\space\text{Identifier} = [\text{Expr}]};\\
      \{[\text{Statement}]^*\}
   \end{cases} \\
   [\text{Expr}] &\to
   \begin{cases}
      [\text{Term}] \\
      [\text{BinExpr}]
   \end{cases}\\
   [\text{BinExpr}] &\to
   \begin{cases}
      [\text{Expr}] ^ [\text{Expr}] & \text{prec}  = 3\\
      [\text{Expr}] * [\text{Expr}] & \text{prec}  = 2\\
      [\text{Expr}] / [\text{Expr}] & \text{prec}  = 2\\
      [\text{Expr}] \% [\text{Expr}] & \text{prec} = 2\\
      [\text{Expr}] + [\text{Expr}] & \text{prec}  = 1\\
      [\text{Expr}] - [\text{Expr}] & \text{prec}  = 1\\
   \end{cases}\\
  [\text{Term}] &\to
   \begin{cases}
      \text{int\_lit} \\
      \text{Identifier}\\
      ([\text{Expr}])\\
   \end{cases}
\end{align}
$$